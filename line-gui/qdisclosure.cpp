/*
 *	Copyright (C) 2011 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "qdisclosure.h"

QDisclosure::QDisclosure(QWidget *parent) :
    QWidget(parent)
{
	mWidget = NULL;

	verticalLayout = new QVBoxLayout(this);
	verticalLayout->setSpacing(3);
	verticalLayout->setContentsMargins(3, 3, 3, 3);

	checkExpand = new QCheckBox(this);
	checkExpand->setText("Test");
	verticalLayout->addWidget(checkExpand);

	//setStyleSheet("QWidget {background-color:red;}");
	//setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	setSizePolicy(checkExpand->sizePolicy());

	checkExpand->setStyleSheet("QCheckBox { border-top-width: 1px; border-top-color: gray; border-top-style: outset; padding-top: 9px; padding-bottom: 9px; } "
							   "QCheckBox::indicator { "
							   "  width: 15px; "
							   "  height: 15px; "
							   "} "
							   "QCheckBox::indicator:unchecked { "
							   "  image: url(:/icons/extra-resources/stylesheet-branch-closed.png); "
							   "} "
							   "QCheckBox::indicator:checked { "
							   "  image: url(:/icons/extra-resources/stylesheet-branch-open.png); "
							   "} ");
	checkExpand->setCheckable(false);

	connect(checkExpand, SIGNAL(toggled(bool)), SLOT(checkToggled(bool)));
}

void QDisclosure::setTitle(QString title)
{
	checkExpand->setText(title);
}

QString QDisclosure::title()
{
	return checkExpand->text();
}

void QDisclosure::setWidget(QWidget *widget)
{
	if (mWidget) {
		verticalLayout->removeWidget(mWidget);
	}
	mWidget = widget;
	if (mWidget) {
		verticalLayout->addWidget(mWidget);
	}

	checkExpand->setCheckable(mWidget != NULL);
	checkToggled(checkExpand->isChecked());
}

QWidget *QDisclosure::widget()
{
	return mWidget;
}

void QDisclosure::checkToggled(bool checked)
{
	if (mWidget && checked) {
		setSizePolicy(mWidget->sizePolicy());
	} else {
		setSizePolicy(checkExpand->sizePolicy());
	}
	if (mWidget)
		mWidget->setVisible(checked);
	updateGeometry();
	emit toggled(checked);
}

void QDisclosure::expand()
{
	checkExpand->setChecked(true);
}

void QDisclosure::collapse()
{
	checkExpand->setChecked(false);
}

bool QDisclosure::isExpanded()
{
	return checkExpand->isChecked();
}

QSize QDisclosure::minimumSizeHint() const
{
	if (mWidget && checkExpand->isChecked()) {
		QSize size1 = checkExpand->minimumSizeHint();
		QSize size2 = mWidget->minimumSizeHint();
		size1 += size2;
		return size1;
	} else {
		return checkExpand->minimumSizeHint();
	}
}

QSize QDisclosure::sizeHint() const
{
	if (mWidget && checkExpand->isChecked()) {
		QSize size1 = checkExpand->sizeHint();
		QSize size2 = mWidget->sizeHint();
		size1 += size2;
		return size1;
	} else {
		return checkExpand->sizeHint();
	}
}
