#include "quickpresenter.h"
#include "quickpresenter_p.h"

#include <limits>

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QMetaMethod>

#include <QtQml/qqml.h>

#include <QtMvvmCore/private/qtmvvm_logging_p.h>

#include <qurlvalidator.h>
namespace {

void qtMvvmQuickInit()
{
	qmlRegisterType<QUrlValidator>("de.skycoder42.QtMvvm.Quick.Private", 1, 0, "UrlValidator");
	QtMvvm::ServiceRegistry::instance()->registerInterface<QtMvvm::IPresenter, QtMvvm::QuickPresenter>(true);
}

void initResources()
{
#ifdef QT_STATIC
	qtMvvmQuickInit();
	Q_INIT_RESOURCE(qtmvvmquick_module);
#endif
}

}
Q_COREAPP_STARTUP_FUNCTION(qtMvvmQuickInit)

using namespace QtMvvm;

QuickPresenter::QuickPresenter(QObject *parent) :
	QObject(parent),
	IPresenter(),
	d(new QuickPresenterPrivate())
{
	initResources();
}

QuickPresenter::~QuickPresenter() {}

void QuickPresenter::addViewSearchDir(const QString &dirUrl)
{
	QuickPresenterPrivate::currentPresenter()->d->searchDirs.prepend(dirUrl);
}

void QuickPresenter::registerViewExplicitly(const QMetaObject *viewModelType, const QUrl &viewUrl)
{
	QuickPresenterPrivate::currentPresenter()->d->explicitMappings.insert(viewModelType, viewUrl);
}

void QuickPresenter::present(QtMvvm::ViewModel *viewModel, const QVariantHash &params, QPointer<QtMvvm::ViewModel> parent)
{
	auto url = findViewUrl(viewModel->metaObject());
	if(!url.isValid())
		throw PresenterException(QByteArrayLiteral("No Url to a QML View found for ") + viewModel->metaObject()->className());

	if(d->qmlPresenter) {
		QMetaObject::invokeMethod(d->qmlPresenter, "present",
								  Q_ARG(QtMvvm::ViewModel*, viewModel),
								  Q_ARG(QVariantHash, params),
								  Q_ARG(QUrl, url),
								  Q_ARG(QPointer<QtMvvm::ViewModel>, parent));
	} else
		throw PresenterException("QML presenter not ready - cannot present yet");
}

void QuickPresenter::showDialog(const QtMvvm::MessageConfig &config, QtMvvm::MessageResult *result)
{
	if(d->qmlPresenter) {
		QMetaObject::invokeMethod(d->qmlPresenter, "showDialog",
								  Q_ARG(QtMvvm::MessageConfig, config),
								  Q_ARG(QtMvvm::MessageResult*, result));
	} else
		throw PresenterException("QML presenter not ready - cannot present yet");
}

bool QuickPresenter::presentToQml(QObject *qmlPresenter, QObject *viewObject)
{
	auto meta = qmlPresenter->metaObject();
	auto index = presentMethodIndex(meta, viewObject);
	QVariant presented = false;
	if(index != -1) {
		meta->method(index).invoke(qmlPresenter, Qt::DirectConnection,
								   Q_RETURN_ARG(QVariant, presented),
								   Q_ARG(QVariant, QVariant::fromValue(viewObject)));
	}
	return presented.toBool();
}

InputViewFactory *QuickPresenter::inputViewFactory() const
{
	return d->inputViewFactory.data();
}

void QuickPresenter::setInputViewFactory(InputViewFactory *inputViewFactory)
{
	d->inputViewFactory.reset(inputViewFactory);
	emit inputViewFactoryChanged(inputViewFactory);
}

QUrl QuickPresenter::findViewUrl(const QMetaObject *viewModelType)
{
	auto currentMeta = viewModelType;
	while(currentMeta && currentMeta->inherits(&ViewModel::staticMetaObject)) {
		if(d->explicitMappings.contains(currentMeta))
			return d->explicitMappings.value(currentMeta);
		else {
			QByteArray cName = currentMeta->className();
			//strip viewmodel
			auto lIndex = cName.lastIndexOf("ViewModel");
			if(lIndex > 0)
				cName.truncate(lIndex);
			//strip namespaces
			lIndex = cName.lastIndexOf("::");
			if(lIndex > 0)
				cName = cName.mid(lIndex + 2);

			QUrl resUrl;
			auto shortest = std::numeric_limits<int>::max();
			for(auto dir : qAsConst(d->searchDirs)) {
				logDebug() << QDir(dir).entryList();
				QDir searchDir(dir,
							   QStringLiteral("%1*.qml").arg(QString::fromLatin1(cName)),
							   QDir::NoSort,
							   QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);
				QDirIterator iterator(searchDir, QDirIterator::Subdirectories);
				while(iterator.hasNext()) {
					iterator.next();
					if(iterator.fileName().size() < shortest) {
						shortest = iterator.fileName().size();
						if(dir.startsWith(QStringLiteral(":"))) {
							resUrl.clear();
							resUrl.setScheme(QStringLiteral("qrc"));
							resUrl.setPath(iterator.filePath().mid(1)); //skip the beginning colon
						} else
							resUrl = QUrl::fromLocalFile(iterator.filePath());
					}
				}
			}
			if(resUrl.isValid()) {
				logDebug() << "Found URL for viewmodel"
						   << viewModelType->className()
						   << "as:" << resUrl;
				return resUrl;
			}
		}

		currentMeta = currentMeta->superClass();
	}

	return QUrl();
}

int QuickPresenter::presentMethodIndex(const QMetaObject *presenterMetaObject, QObject *viewObject)
{
	auto index = -1;
	if(viewObject->inherits("QQuickPopup"))
		index = presenterMetaObject->indexOfMethod("presentPopup(QVariant)");
	if(viewObject->inherits("QQuickItem")) {
		if(nameOrClassContains(viewObject, QStringLiteral("Drawer")))
			index = presenterMetaObject->indexOfMethod("presentDrawerContent(QVariant)");
		if(index == -1 && nameOrClassContains(viewObject, QStringLiteral("Tab")))
			index = presenterMetaObject->indexOfMethod("presentTab(QVariant)");

		if(index == -1)
			index = presenterMetaObject->indexOfMethod("presentItem(QVariant)");
	}
	return index;
}

bool QuickPresenter::nameOrClassContains(const QObject *obj, const QString &contained, Qt::CaseSensitivity caseSensitive) const
{
	if(obj->objectName().contains(contained, caseSensitive))
		return true;
	auto currentMeta = obj->metaObject();
	while(currentMeta) {
		if(QString::fromUtf8(currentMeta->className()).contains(contained, caseSensitive))
			return true;
		currentMeta = currentMeta->superClass();
	}
	return false;
}

// ------------- Private Implementation -------------

QuickPresenterPrivate::QuickPresenterPrivate() :
	explicitMappings(),
	searchDirs({QStringLiteral(":/qtmvvm/views")}),
	qmlPresenter(nullptr),
	inputViewFactory(new InputViewFactory())
{}

QuickPresenter *QuickPresenterPrivate::currentPresenter()
{
	try {
#ifndef Q_NO_DEBUG
		Q_ASSERT_X(dynamic_cast<QuickPresenter*>(ServiceRegistry::instance()->service<IPresenter>()),
				   Q_FUNC_INFO,
				   "Cannot register views if the current presenter does not extend QtMvvm::QuickPresenter");
#endif
		return static_cast<QuickPresenter*>(ServiceRegistry::instance()->service<IPresenter>());
	} catch(QException &e) {
		qFatal(e.what());
	}
}

void QuickPresenterPrivate::setQmlPresenter(QObject *presenter)
{
	currentPresenter()->d->qmlPresenter = presenter;
}