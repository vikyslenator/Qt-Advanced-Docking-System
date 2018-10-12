//============================================================================
/// \file   DockAreaTabBar.cpp
/// \author Uwe Kindler
/// \date   24.08.2018
/// \brief  Implementation of CDockAreaTabBar class
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include "DockAreaTabBar.h"

#include <QMouseEvent>
#include <QScrollBar>
#include <QDebug>
#include <QBoxLayout>
#include <QMenu>

#include "FloatingDockContainer.h"
#include "DockAreaWidget.h"
#include "DockOverlay.h"
#include "DockManager.h"
#include "DockWidget.h"
#include "DockWidgetTab.h"

#include <iostream>

namespace ads
{
/**
 * Private data class of CDockAreaTabBar class (pimpl)
 */
struct DockAreaTabBarPrivate
{
	CDockAreaTabBar* _this;
	QPoint DragStartMousePos;
	CDockAreaWidget* DockArea;
	CFloatingDockContainer* FloatingWidget = nullptr;
	QWidget* TabsContainerWidget;
	QBoxLayout* TabsLayout;
	int CurrentIndex = -1;
	bool MenuOutdated = true;
	QMenu* TabsMenu;

	/**
	 * Private data constructor
	 */
	DockAreaTabBarPrivate(CDockAreaTabBar* _public);
};
// struct DockAreaTabBarPrivate

//============================================================================
DockAreaTabBarPrivate::DockAreaTabBarPrivate(CDockAreaTabBar* _public) :
	_this(_public)
{

}

//============================================================================
CDockAreaTabBar::CDockAreaTabBar(CDockAreaWidget* parent) :
	QScrollArea(parent),
	d(new DockAreaTabBarPrivate(this))
{
	d->DockArea = parent;
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
	setFrameStyle(QFrame::NoFrame);
	setWidgetResizable(true);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	d->TabsContainerWidget = new QWidget();
	d->TabsContainerWidget->setObjectName("tabsContainerWidget");
	setWidget(d->TabsContainerWidget);

	d->TabsLayout = new QBoxLayout(QBoxLayout::LeftToRight);
	d->TabsLayout->setContentsMargins(0, 0, 0, 0);
	d->TabsLayout->setSpacing(0);
	d->TabsLayout->addStretch(1);
	d->TabsContainerWidget->setLayout(d->TabsLayout);
}

//============================================================================
CDockAreaTabBar::~CDockAreaTabBar()
{
	delete d;
}


//============================================================================
void CDockAreaTabBar::wheelEvent(QWheelEvent* Event)
{
	Event->accept();
	const int direction = Event->angleDelta().y();
	if (direction < 0)
	{
		horizontalScrollBar()->setValue(horizontalScrollBar()->value() + 20);
	}
	else
	{
		horizontalScrollBar()->setValue(horizontalScrollBar()->value() - 20);
	}
}


//============================================================================
void CDockAreaTabBar::mousePressEvent(QMouseEvent* ev)
{
	std::cout << "CDockAreaTabBar::mousePressEvent" << std::endl;
	if (ev->button() == Qt::LeftButton)
	{
		ev->accept();
		d->DragStartMousePos = ev->pos();
		return;
	}
	QScrollArea::mousePressEvent(ev);
}


//============================================================================
void CDockAreaTabBar::mouseReleaseEvent(QMouseEvent* ev)
{
	if (ev->button() == Qt::LeftButton)
	{
		qDebug() << "CTabsScrollArea::mouseReleaseEvent";
		ev->accept();
		d->FloatingWidget = nullptr;
		d->DragStartMousePos = QPoint();
		return;
	}
	QScrollArea::mouseReleaseEvent(ev);
}


//============================================================================
void CDockAreaTabBar::mouseMoveEvent(QMouseEvent* ev)
{
	QScrollArea::mouseMoveEvent(ev);
	if (ev->buttons() != Qt::LeftButton)
	{
		return;
	}

	if (d->FloatingWidget)
	{
		d->FloatingWidget->moveFloating();
		return;
	}

	// If this is the last dock area in a dock container it does not make
	// sense to move it to a new floating widget and leave this one
	// empty
	if (d->DockArea->dockContainer()->isFloating()
	 && d->DockArea->dockContainer()->visibleDockAreaCount() == 1)
	{
		return;
	}

	if (!this->geometry().contains(ev->pos()))
	{
		qDebug() << "CTabsScrollArea::startFloating";
		startFloating(d->DragStartMousePos);
		auto Overlay = d->DockArea->dockManager()->containerOverlay();
		Overlay->setAllowedAreas(OuterDockAreas);
	}

	return;
}


//============================================================================
void CDockAreaTabBar::mouseDoubleClickEvent(QMouseEvent *event)
{
	// If this is the last dock area in a dock container it does not make
	// sense to move it to a new floating widget and leave this one
	// empty
	if (d->DockArea->dockContainer()->isFloating() && d->DockArea->dockContainer()->dockAreaCount() == 1)
	{
		return;
	}
	startFloating(event->pos());
}


//============================================================================
void CDockAreaTabBar::startFloating(const QPoint& Pos)
{
	QSize Size = d->DockArea->size();
	CFloatingDockContainer* FloatingWidget = new CFloatingDockContainer(d->DockArea);
	FloatingWidget->startFloating(Pos, Size);
	d->FloatingWidget = FloatingWidget;
	auto TopLevelDockWidget = d->FloatingWidget->topLevelDockWidget();
	if (TopLevelDockWidget)
	{
		TopLevelDockWidget->emitTopLevelChanged(true);
	}
}


//============================================================================
void CDockAreaTabBar::setCurrentIndex(int index)
{
	std::cout << "CDockAreaTabBar::setCurrentIndex " << index << std::endl;
	if (index == d->CurrentIndex)
	{
		return;
	}

	if (index < 0 || index > (count() - 1))
	{
		qWarning() << Q_FUNC_INFO << "Invalid index" << index;
		return;
    }

    emit currentChanging(index);

	// Set active TAB and update all other tabs to be inactive
	for (int i = 0; i < count(); ++i)
	{
		QLayoutItem* item = d->TabsLayout->itemAt(i);
		if (!item->widget())
		{
			continue;
		}

		auto TabWidget = dynamic_cast<CDockWidgetTab*>(item->widget());
		if (!TabWidget)
		{
			continue;
		}

		if (i == index)
		{
			TabWidget->show();
			TabWidget->setActiveTab(true);
			ensureWidgetVisible(TabWidget);
		}
		else
		{
			TabWidget->setActiveTab(false);
		}
	}

	d->CurrentIndex = index;
	emit currentChanged(index);
}


//============================================================================
int CDockAreaTabBar::count() const
{
	// The tab bar contains a stretch item as last item
	return d->TabsLayout->count() - 1;
}


//===========================================================================
void CDockAreaTabBar::insertTab(int Index, CDockWidgetTab* Tab)
{
	d->TabsLayout->insertWidget(Index, Tab);
	connect(Tab, SIGNAL(clicked()), this, SLOT(onTabClicked()));
	connect(Tab, SIGNAL(moved(const QPoint&)), this, SLOT(onTabWidgetMoved(const QPoint&)));
	d->MenuOutdated = true;
	if (Index <= d->CurrentIndex)
	{
		d->CurrentIndex++;
	}
}


//===========================================================================
void CDockAreaTabBar::removeTab(CDockWidgetTab* Tab)
{
	std::cout << "CDockAreaTabBar::removeTab " << std::endl;
	d->TabsLayout->removeWidget(Tab);
	Tab->disconnect(this);
	d->MenuOutdated = true;
}


//===========================================================================
int CDockAreaTabBar::currentIndex() const
{
	return d->CurrentIndex;
}


//===========================================================================
CDockWidgetTab* CDockAreaTabBar::currentTab() const
{
	return qobject_cast<CDockWidgetTab*>(d->TabsLayout->itemAt(d->CurrentIndex)->widget());
}


//===========================================================================
void CDockAreaTabBar::onTabClicked()
{
	CDockWidgetTab* Tab = qobject_cast<CDockWidgetTab*>(sender());
	if (!Tab)
	{
		return;
	}

	int index = d->TabsLayout->indexOf(Tab);
	if (index < 0)
	{
		return;
	}
	setCurrentIndex(index);
	std::cout << "emit tabBarClicked " << index << std::endl;
 	emit tabBarClicked(index);
}


//===========================================================================
CDockWidgetTab* CDockAreaTabBar::tab(int Index) const
{
	if (Index >= count())
	{
		return 0;
	}
	return qobject_cast<CDockWidgetTab*>(d->TabsLayout->itemAt(Index)->widget());
}


//===========================================================================
void CDockAreaTabBar::onTabWidgetMoved(const QPoint& GlobalPos)
{
	CDockWidgetTab* MovingTab = qobject_cast<CDockWidgetTab*>(sender());
	if (!MovingTab)
	{
		return;
	}

	int fromIndex = d->TabsLayout->indexOf(MovingTab);
	auto MousePos = mapFromGlobal(GlobalPos);
	int toIndex = -1;
	// Find tab under mouse
	for (int i = 0; i < count(); ++i)
	{
		CDockWidgetTab* DropTab = tab(i);
		if (DropTab == MovingTab || !DropTab->isVisibleTo(this)
		    || !DropTab->geometry().contains(MousePos))
		{
			continue;
		}

		toIndex = d->TabsLayout->indexOf(DropTab);
		if (toIndex == fromIndex)
		{
			toIndex = -1;
			continue;
		}

		if (toIndex < 0)
		{
			toIndex = 0;
		}
		break;
	}

	// Now check if the mouse is behind the last tab
	if (toIndex < 0)
	{
		if (MousePos.x() > tab(count() - 1)->geometry().right())
		{
			qDebug() << "after all tabs";
			toIndex = count() - 1;
		}
		else
		{
			toIndex = fromIndex;
		}
	}

	d->TabsLayout->removeWidget(MovingTab);
	d->TabsLayout->insertWidget(toIndex, MovingTab);
	if (toIndex >= 0)
	{
		qDebug() << "tabMoved from " << fromIndex << " to " << toIndex;
		d->CurrentIndex = toIndex;
		emit tabMoved(fromIndex, toIndex);
		emit currentChanged(toIndex);
	}
}


//===========================================================================
void CDockAreaTabBar::closeTab(int Index)
{
	if (Index < 0 || Index >= count())
	{
		return;
	}
	emit tabCloseRequested(Index);
}
} // namespace ads

//---------------------------------------------------------------------------
// EOF DockAreaTabBar.cpp
