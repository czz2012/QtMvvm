#ifndef QTMVVM_QUICKPRESENTER_H
#define QTMVVM_QUICKPRESENTER_H

#include <QtCore/qobject.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qurl.h>
#include <QtCore/qmetaobject.h>

#include <QtMvvmCore/ipresenter.h>
#include <QtMvvmCore/serviceregistry.h>

#include "QtMvvmQuick/qtmvvmquick_global.h"
#include "QtMvvmQuick/inputviewfactory.h"

namespace QtMvvm {

class QuickPresenterPrivate;
class Q_MVVMQUICK_EXPORT QuickPresenter : public QObject, public IPresenter
{
	Q_OBJECT
	Q_INTERFACES(QtMvvm::IPresenter)

	Q_PROPERTY(InputViewFactory* inputViewFactory READ inputViewFactory WRITE setInputViewFactory NOTIFY inputViewFactoryChanged)

public:
	Q_INVOKABLE explicit QuickPresenter(QObject *parent = nullptr);
	~QuickPresenter();

	template <typename TPresenter = QuickPresenter>
	static void registerAsPresenter();

	static void addViewSearchDir(const QString &dirUrl);
	template <typename TViewModel>
	static void registerViewExplicitly(const QUrl &viewUrl);
	static void registerViewExplicitly(const QMetaObject *viewModelType, const QUrl &viewUrl);

	void present(ViewModel *viewModel, const QVariantHash &params, QPointer<ViewModel> parent) override;
	void showDialog(const MessageConfig &config, MessageResult *result) override;

	virtual bool presentToQml(QObject *qmlPresenter, QObject *viewObject);

	InputViewFactory* inputViewFactory() const;

public Q_SLOTS:
	void setInputViewFactory(InputViewFactory* inputViewFactory);

Q_SIGNALS:
	void inputViewFactoryChanged(InputViewFactory* inputViewFactory);

protected:
	virtual QUrl findViewUrl(const QMetaObject *viewModelType);
	virtual int presentMethodIndex(const QMetaObject *presenterMetaObject, QObject *viewObject);

	bool nameOrClassContains(const QObject *obj, const QString &contained, Qt::CaseSensitivity caseSensitive = Qt::CaseInsensitive) const;

private:
	friend class QtMvvm::QuickPresenterPrivate;
	QScopedPointer<QuickPresenterPrivate> d;
};

template<typename TPresenter>
void QuickPresenter::registerAsPresenter()
{
	static_assert(std::is_base_of<QuickPresenter, TPresenter>::value, "TPresenter must inherit QtMvvm::QuickPresenter!");
	ServiceRegistry::instance()->registerInterface<IPresenter, TPresenter>();
}

template<typename TViewModel>
void QuickPresenter::registerViewExplicitly(const QUrl &viewUrl)
{
	static_assert(std::is_base_of<ViewModel, TViewModel>::value, "TViewModel must inherit ViewModel!");
	registerViewExplicitly(&TViewModel::staticMetaObject, viewUrl);
}


}

#endif // QTMVVM_QUICKPRESENTER_H