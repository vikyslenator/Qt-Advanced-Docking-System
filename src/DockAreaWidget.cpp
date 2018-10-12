/*******************************************************************************
** Qt Advanced Docking System
** Copyright (C) 2017 Uwe Kindler
** 
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
** 
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
** 
** You should have received a copy of the GNU Lesser General Public
** License along with this library; If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/


//============================================================================
/// \file   DockAreaWidget.cpp
/// \author Uwe Kindler
/// \date   24.02.2017
/// \brief  Implementation of CDockAreaWidget class
//============================================================================


//============================================================================
//                                   INCLUDES
//============================================================================
#include <DockWidgetTab.h>
#include "DockAreaWidget.h"

#include <QStackedLayout>
#include <QScrollBar>
#include <QScrollArea>
#include <QWheelEvent>
#include <QStyle>
#include <QPushButton>
#include <QDebug>
#include <QMenu>
#include <QSplitter>
#include <QXmlStreamWriter>
#include <QVector>
#include <QList>


#include "DockContainerWidget.h"
#include "DockWidget.h"
#include "FloatingDockContainer.h"
#include "DockManager.h"
#include "DockOverlay.h"
#include "DockAreaTabBar.h"
#include "DockSplitter.h"

#include <iostream>


namespace ads
{
static const char* const INDEX_PROPERTY = "index";
static const char* const ACTION_PROPERTY = "action";
static const char* const DOCKWIDGET_PROPERTY = "dockwidget";
static const int APPEND = -1;


/**
 * Default stack area layout
 */
class CStackedDockAreaLayout
{
private:
	QStackedLayout* Layout;

public:
	CStackedDockAreaLayout(QBoxLayout* ParentLayout)
	{
		Layout = new QStackedLayout();
		Layout->setContentsMargins(0, 0, 0, 0);
		Layout->setSpacing(0);
		Layout->setSizeConstraint(QLayout::SetNoConstraint);
		ParentLayout->addLayout(Layout, 1);
	}

	int count() const
	{
		return Layout->count();
	}

	void insertWidget(int index, QWidget* Widget)
	{
		Layout->insertWidget(index, Widget);
	}

	void removeWidget(QWidget* Widget)
	{
		Layout->removeWidget(Widget);
	}

	void setCurrentIndex(int Index)
	{
		Layout->setCurrentIndex(Index);
	}

	int currentIndex() const
	{
		return Layout->currentIndex();
	}

	QWidget* currentWidget() const
	{
		return Layout->currentWidget();
	}

	bool isEmpty() const
	{
		return Layout->isEmpty();
	}

	int indexOf(QWidget* w) const
	{
		return Layout->indexOf(w);
	}

	QWidget* widget(int index) const
	{
		return Layout->widget(index);
	}

	QRect geometry() const
	{
		return Layout->geometry();
	}
};


/**
 * New dock area layout
 */
class CDockAreaLayout
{
private:
	QBoxLayout* m_ParentLayout;
	QList<QWidget*> m_Widgets;
	int m_CurrentIndex = -1;
	QWidget* m_CurrentWidget = nullptr;

public:
	CDockAreaLayout(QBoxLayout* ParentLayout)
		: m_ParentLayout(ParentLayout)
	{

	}

	int count() const
	{
		return m_Widgets.count();
	}

	void insertWidget(int index, QWidget* Widget)
	{
		Widget->setParent(0);
		if (index < 0)
		{
			index = m_Widgets.count();
		}
		m_Widgets.insert(index, Widget);
		if (m_CurrentIndex < 0)
		{
			setCurrentIndex(index);
		}
		else
		{
			if (index <= m_CurrentIndex )
			{
				++m_CurrentIndex;
			}
		}
	}

	void removeWidget(QWidget* Widget)
	{
		if (currentWidget() == Widget)
		{
			auto LayoutItem = m_ParentLayout->takeAt(1);
			if (LayoutItem)
			{
				LayoutItem->widget()->setParent(0);
			}
		}
		m_Widgets.removeOne(Widget);
		//setCurrentIndex(0);
	}

	QWidget* currentWidget() const
	{
		return m_CurrentWidget;
	}

	void setCurrentIndex(int index)
	{
		std::cout << "CDockAreaLayout::setCurrentIndex " << index << std::endl;
		QWidget *prev = currentWidget();
		QWidget *next = widget(index);
		if (!next || (next == prev && !m_CurrentWidget))
		{
			std::cout << "return" << std::endl;
			return;
		}

		bool reenableUpdates = false;
		QWidget *parent = m_ParentLayout->parentWidget();

		if (parent && parent->updatesEnabled())
		{
			reenableUpdates = true;
			parent->setUpdatesEnabled(false);
		}

		std::cout << "m_ParentLayout->addWidget(next)" << std::endl;
		auto LayoutItem = m_ParentLayout->takeAt(1);
		if (LayoutItem)
		{
			LayoutItem->widget()->setParent(0);
		}

		m_ParentLayout->addWidget(next);
		if (prev)
		{
			prev->hide();
		}
		m_CurrentIndex = index;
		m_CurrentWidget = next;


		if (reenableUpdates)
		{
			parent->setUpdatesEnabled(true);
		}
	}

	int currentIndex() const
	{
		return m_CurrentIndex;
	}

	bool isEmpty() const
	{
		return m_Widgets.empty();
	}

	int indexOf(QWidget* w) const
	{
		return m_Widgets.indexOf(w);
	}

	QWidget* widget(int index) const
	{
		return (index < m_Widgets.size()) ? m_Widgets.at(index) : nullptr;
	}

	QRect geometry() const
	{
		return m_Widgets.empty() ? QRect() : currentWidget()->geometry();
	}
};



using DockAreaLayout = CDockAreaLayout;


/**
 * Private data class of CDockAreaWidget class (pimpl)
 */
struct DockAreaWidgetPrivate
{
	CDockAreaWidget* _this;
	QBoxLayout* Layout;
	QFrame* TitleBar;
	QBoxLayout* TopLayout;
	DockAreaLayout* ContentsLayout;
	CDockAreaTabBar* TabBar;
	QPushButton* TabsMenuButton;
	QPushButton* CloseButton;
	//int TabsLayoutInitCount;
	CDockManager* DockManager = nullptr;
	bool MenuOutdated = true;

	/**
	 * Private data constructor
	 */
	DockAreaWidgetPrivate(CDockAreaWidget* _public);

	/**
	 * Creates the layout for top area with tabs and close button
	 */
	void createTabBar();

	/**
	 * Returns the dock widget with the given index
	 */
	CDockWidget* dockWidgetAt(int index)
	{
		return dynamic_cast<CDockWidget*>(ContentsLayout->widget(index));
	}

	/**
	 * Convenience function to ease title widget access by index
	 */
	CDockWidgetTab* tabWidgetAt(int index)
	{
		return dockWidgetAt(index)->tabWidget();
	}

	/**
	 * Adds a tabs menu entry for the given dock widget
	 * If menu is 0, a menu entry is added to the menu of the TabsMenuButton
	 * member. If menu is a valid menu pointer, the entry will be added to
	 * the given menu
	 */
	void addTabsMenuEntry(CDockWidget* DockWidget, int Index = -1, QMenu* menu = 0);

	/**
	 * Returns the tab action of the given dock widget
	 */
	QAction* dockWidgetTabAction(CDockWidget* DockWidget) const
	{
		return qvariant_cast<QAction*>(DockWidget->property(ACTION_PROPERTY));
	}

	/**
	 * Returns the index of the given dock widget
	 */
	int dockWidgetIndex(CDockWidget* DockWidget) const
	{
		return DockWidget->property(INDEX_PROPERTY).toInt();
	}

	/**
	 * Update the tabs menu if dock widget order changed or if dock widget has
	 * been removed
	 */
	void markTabsMenuOutdated();

	/**
	 * Updates the tabs menu if it is outdated
	 */
	void updateTabsMenu();

	/**
	 * Updates the tab bar visibility depending on the number of dock widgets
	 * in this area
	 */
	void updateTabBar();
};
// struct DockAreaWidgetPrivate


//============================================================================
DockAreaWidgetPrivate::DockAreaWidgetPrivate(CDockAreaWidget* _public) :
	_this(_public)
{

}


//============================================================================
void DockAreaWidgetPrivate::createTabBar()
{
	TitleBar = new QFrame(_this);
	TitleBar->setObjectName("dockAreaTitleBar");
	TopLayout = new QBoxLayout(QBoxLayout::LeftToRight);
	TopLayout->setContentsMargins(0, 0, 0, 0);
	TopLayout->setSpacing(0);
	TitleBar->setLayout(TopLayout);
	Layout->addWidget(TitleBar);
	TitleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	TabBar = new CDockAreaTabBar(_this);
	TopLayout->addWidget(TabBar, 1);
	_this->connect(TabBar, SIGNAL(tabBarClicked(int)), SLOT(setCurrentIndex(int)));
	_this->connect(TabBar, SIGNAL(tabMoved(int, int)), SLOT(reorderDockWidget(int, int)));

	TabsMenuButton = new QPushButton();
	TabsMenuButton->setObjectName("tabsMenuButton");
	TabsMenuButton->setFlat(true);
	TabsMenuButton->setIcon(_this->style()->standardIcon(QStyle::SP_TitleBarUnshadeButton));
	TabsMenuButton->setMaximumWidth(TabsMenuButton->iconSize().width());
	QMenu* TabsMenu = new QMenu(TabsMenuButton);
	_this->connect(TabsMenu, SIGNAL(aboutToShow()), SLOT(onTabsMenuAboutToShow()));
	TabsMenuButton->setMenu(TabsMenu);
	TopLayout->addWidget(TabsMenuButton, 0);
	TabsMenuButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	_this->connect(TabsMenuButton->menu(), SIGNAL(triggered(QAction*)),
		SLOT(onTabsMenuActionTriggered(QAction*)));

	CloseButton = new QPushButton();
	CloseButton->setObjectName("closeButton");
	CloseButton->setFlat(true);
	CloseButton->setIcon(_this->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
	CloseButton->setToolTip(_this->tr("Close"));
	CloseButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	TopLayout->addWidget(CloseButton, 0);
	_this->connect(CloseButton, SIGNAL(clicked()), SLOT(onCloseButtonClicked()));
}


//============================================================================
void DockAreaWidgetPrivate::updateTabBar()
{
	CDockContainerWidget* Container = _this->dockContainer();
	if (!Container)
	{
		return;
	}

	TitleBar->setVisible(!Container->isFloating() || !Container->hasTopLevelDockWidget());
}


//============================================================================
void DockAreaWidgetPrivate::addTabsMenuEntry(CDockWidget* DockWidget,
	int Index, QMenu* menu)
{
	menu = menu ? menu : TabsMenuButton->menu();
	QAction* Action;
	if (Index >= 0 && Index < menu->actions().count())
	{
		Action = new QAction(DockWidget->icon(), DockWidget->windowTitle());
		menu->insertAction(menu->actions().at(Index), Action);
	}
	else
	{
		Action = menu->addAction(DockWidget->icon(), DockWidget->windowTitle());
	}
	Action->setProperty(DOCKWIDGET_PROPERTY, QVariant::fromValue(DockWidget));
	QVariant vAction = QVariant::fromValue(Action);
	DockWidget->setProperty(ACTION_PROPERTY, vAction);
}


//============================================================================
void DockAreaWidgetPrivate::markTabsMenuOutdated()
{
	MenuOutdated = true;
}


//============================================================================
void DockAreaWidgetPrivate::updateTabsMenu()
{
	if (!MenuOutdated)
	{
		return;
	}

	QMenu* menu = TabsMenuButton->menu();
	menu->clear();
	for (int i = 0; i < ContentsLayout->count(); ++i)
	{
		if (dockWidgetAt(i)->isClosed())
		{
			continue;
		}
		addTabsMenuEntry(dockWidgetAt(i), APPEND, menu);
	}

	MenuOutdated = false;
}


//============================================================================
CDockAreaWidget::CDockAreaWidget(CDockManager* DockManager, CDockContainerWidget* parent) :
	QFrame(parent),
	d(new DockAreaWidgetPrivate(this))
{
	d->DockManager = DockManager;
	d->Layout = new QBoxLayout(QBoxLayout::TopToBottom);
	d->Layout->setContentsMargins(0, 0, 0, 0);
	d->Layout->setSpacing(0);
	setLayout(d->Layout);

	d->createTabBar();
	d->ContentsLayout = new DockAreaLayout(d->Layout);
}

//============================================================================
CDockAreaWidget::~CDockAreaWidget()
{
	qDebug() << "~CDockAreaWidget()";
	delete d->ContentsLayout;
	delete d;
}


//============================================================================
CDockManager* CDockAreaWidget::dockManager() const
{
	return d->DockManager;
}


//============================================================================
CDockContainerWidget* CDockAreaWidget::dockContainer() const
{
	return internal::findParent<CDockContainerWidget*>(this);
}


//============================================================================
void CDockAreaWidget::addDockWidget(CDockWidget* DockWidget)
{
	insertDockWidget(d->ContentsLayout->count(), DockWidget);
}


//============================================================================
void CDockAreaWidget::insertDockWidget(int index, CDockWidget* DockWidget,
	bool Activate)
{
	d->ContentsLayout->insertWidget(index, DockWidget);
	DockWidget->tabWidget()->setDockAreaWidget(this);
	auto TabWidget = DockWidget->tabWidget();
	d->TabBar->insertTab(index, TabWidget);
	TabWidget->setVisible(!DockWidget->isClosed());
	DockWidget->setProperty(INDEX_PROPERTY, index);
	d->markTabsMenuOutdated();
	if (Activate)
	{
		setCurrentIndex(index);
	}
	DockWidget->setDockArea(this);
}


//============================================================================
void CDockAreaWidget::removeDockWidget(CDockWidget* DockWidget)
{
	qDebug() << "CDockAreaWidget::removeDockWidget";
	auto NextOpenDockWidget = nextOpenDockWidget(DockWidget);

	d->ContentsLayout->removeWidget(DockWidget);
	auto TabWidget = DockWidget->tabWidget();
	TabWidget->hide();
	d->TabBar->removeTab(TabWidget);
	if (NextOpenDockWidget)
	{
		setCurrentDockWidget(NextOpenDockWidget);
		d->markTabsMenuOutdated();
	}
	else if (d->ContentsLayout->isEmpty())
	{
		qDebug() << "Dock Area empty";
		dockContainer()->removeDockArea(this);
		this->deleteLater();
	}
	else
	{
		// if contents layout is not empty but there are no more open dock
		// widgets, then we need to hide the dock area because it does not
		// contain any visible content
		hideAreaWithNoVisibleContent();
	}

	d->updateTabBar();
	DockWidget->setDockArea(nullptr);

#if (ADS_DEBUG_LEVEL > 0)
	CDockContainerWidget* DockContainer = dockContainer();
	DockContainer->dumpLayout();
#endif
}


//============================================================================
void CDockAreaWidget::hideAreaWithNoVisibleContent()
{
	this->hide();

	// Hide empty parent splitter
	auto Splitter = internal::findParent<CDockSplitter*>(this);
	while (Splitter && Splitter->isVisible())
	{
		if (!Splitter->hasVisibleContent())
		{
			Splitter->hide();
		}
		Splitter = internal::findParent<CDockSplitter*>(Splitter);
	}

	//Hide empty floating widget
	CDockContainerWidget* Container = this->dockContainer();
	if (Container->isFloating() && Container->openedDockAreas().isEmpty())
	{
		CFloatingDockContainer* FloatingWidget = internal::findParent<CFloatingDockContainer*>(Container);
		FloatingWidget->hide();
	}
}


//============================================================================
void CDockAreaWidget::hideAreaIfNoVisibleContent()
{
	if (openedDockWidgets().isEmpty())
	{
		hideAreaIfNoVisibleContent();
	}
}


//============================================================================
void CDockAreaWidget::onCloseButtonClicked()
{
	currentDockWidget()->toggleView(false);
}


//============================================================================
CDockWidget* CDockAreaWidget::currentDockWidget() const
{
	return dockWidget(currentIndex());
}


//============================================================================
void CDockAreaWidget::setCurrentDockWidget(CDockWidget* DockWidget)
{
	int Index = index(DockWidget);
	if (Index < 0)
	{
		return;
	}

	if (dockManager()->isRestoringState())
	{
		return;
	}

	setCurrentIndex(Index);
}


//============================================================================
void CDockAreaWidget::setCurrentIndex(int index)
{
	std::cout << "CDockAreaWidget::setCurrentIndex " << index << std::endl;
	if (index < 0 || index > (d->TabBar->count() - 1))
	{
		qWarning() << Q_FUNC_INFO << "Invalid index" << index;
		return;
    }

    emit currentChanging(index);
    d->TabBar->setCurrentIndex(index);
    CDockWidgetTab* CurrentTab = d->TabBar->currentTab();
	auto Features = CurrentTab->dockWidget()->features();
	d->CloseButton->setVisible(Features.testFlag(CDockWidget::DockWidgetClosable));

	d->ContentsLayout->setCurrentIndex(index);
	d->ContentsLayout->currentWidget()->show();
	emit currentChanged(index);
}


//============================================================================
int CDockAreaWidget::currentIndex() const
{
	return d->ContentsLayout->currentIndex();
}


//============================================================================
QRect CDockAreaWidget::titleAreaGeometry() const
{
	return d->TopLayout->geometry();
}

//============================================================================
QRect CDockAreaWidget::contentAreaGeometry() const
{
	return d->ContentsLayout->geometry();
}


//============================================================================
int CDockAreaWidget::index(CDockWidget* DockWidget)
{
	return d->ContentsLayout->indexOf(DockWidget);
}


//============================================================================
QList<CDockWidget*> CDockAreaWidget::dockWidgets() const
{
	QList<CDockWidget*> DockWidgetList;
	for (int i = 0; i < d->ContentsLayout->count(); ++i)
	{
		DockWidgetList.append(dockWidget(i));
	}
	return DockWidgetList;
}


//============================================================================
int CDockAreaWidget::openDockWidgetsCount() const
{
	int Count = 0;
	for (int i = 0; i < d->ContentsLayout->count(); ++i)
	{
		if (!dockWidget(i)->isClosed())
		{
			++Count;
		}
	}
	return Count;
}


//============================================================================
QList<CDockWidget*> CDockAreaWidget::openedDockWidgets() const
{
	QList<CDockWidget*> DockWidgetList;
	for (int i = 0; i < d->ContentsLayout->count(); ++i)
	{
		CDockWidget* DockWidget = dockWidget(i);
		if (!DockWidget->isClosed())
		{
			DockWidgetList.append(dockWidget(i));
		}
	}
	return DockWidgetList;
}


//============================================================================
int CDockAreaWidget::indexOfContentByTitlePos(const QPoint& p, QWidget* exclude) const
{
	for (int i = 0; i < d->ContentsLayout->count(); ++i)
	{
		auto TabWidget = d->tabWidgetAt(i);
		if (TabWidget->isVisible() && TabWidget->geometry().contains(p) && (!exclude || TabWidget != exclude))
		{
			return i;
		}
	}
	return -1;
}


//============================================================================
int CDockAreaWidget::dockWidgetsCount() const
{
	return d->ContentsLayout->count();
}


//============================================================================
CDockWidget* CDockAreaWidget::dockWidget(int Index) const
{
	return dynamic_cast<CDockWidget*>(d->ContentsLayout->widget(Index));
}


//============================================================================
void CDockAreaWidget::reorderDockWidget(int fromIndex, int toIndex)
{
	if (fromIndex >= d->ContentsLayout->count() || fromIndex < 0
     || toIndex >= d->ContentsLayout->count() || toIndex < 0 || fromIndex == toIndex)
	{
		qDebug() << "Invalid index for tab movement" << fromIndex << toIndex;
		return;
	}

	auto Widget = d->ContentsLayout->widget(fromIndex);
	d->ContentsLayout->removeWidget(Widget);
	d->ContentsLayout->insertWidget(toIndex, Widget);
	setCurrentIndex(d->TabBar->currentIndex());
}


//============================================================================
void CDockAreaWidget::toggleDockWidgetView(CDockWidget* DockWidget, bool Open)
{
	Q_UNUSED(DockWidget);
	Q_UNUSED(Open);
	updateTabBarVisibility();
	d->markTabsMenuOutdated();
}


//============================================================================
void CDockAreaWidget::onTabsMenuActionTriggered(QAction* Action)
{
	QVariant vDockWidget = Action->property(DOCKWIDGET_PROPERTY);
	CDockWidget* DockWidget = vDockWidget.value<CDockWidget*>();
	setCurrentDockWidget(DockWidget);
}


//============================================================================
void CDockAreaWidget::updateTabBarVisibility()
{
	d->updateTabBar();
}


//============================================================================
void CDockAreaWidget::saveState(QXmlStreamWriter& s) const
{
	s.writeStartElement("DockAreaWidget");
	s.writeAttribute("Tabs", QString::number(d->ContentsLayout->count()));
	s.writeAttribute("CurrentIndex", QString::number(d->ContentsLayout->currentIndex()));
	qDebug() << "CDockAreaWidget::saveState TabCount: " << d->ContentsLayout->count()
			<< " CurrentIndex: " << d->ContentsLayout->currentIndex();
	for (int i = 0; i < d->ContentsLayout->count(); ++i)
	{
		dockWidget(i)->saveState(s);
	}
	s.writeEndElement();
}


//============================================================================
CDockWidget* CDockAreaWidget::nextOpenDockWidget(CDockWidget* DockWidget) const
{
	auto OpenDockWidgets = openedDockWidgets();
	if (OpenDockWidgets.count() > 1 || (OpenDockWidgets.count() == 1 && OpenDockWidgets[0] != DockWidget))
	{
		CDockWidget* NextDockWidget;
		if (OpenDockWidgets.last() == DockWidget)
		{
			NextDockWidget = OpenDockWidgets[OpenDockWidgets.count() - 2];
		}
		else
		{
			int NextIndex = OpenDockWidgets.indexOf(DockWidget) + 1;
			NextDockWidget = OpenDockWidgets[NextIndex];
		}

		return NextDockWidget;
	}
	else
	{
		return nullptr;
	}
}


//============================================================================
void CDockAreaWidget::onTabsMenuAboutToShow()
{
	d->updateTabsMenu();
}

} // namespace ads

//---------------------------------------------------------------------------
// EOF DockAreaWidget.cpp
