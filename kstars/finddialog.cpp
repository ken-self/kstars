/***************************************************************************
                          finddialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <klocale.h>
#include <qframe.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlistbox.h>
#include <qcombobox.h>
#include <qpushbutton.h>

#include "finddialog.h"
#include "kstars.h"

#include <qglobal.h>
#if (QT_VERSION > 299)
#include <qptrlist.h>
#endif

FindDialog::FindDialog( QWidget* parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Find Object" ), Ok|Cancel, Ok, parent ) {
	
	QFrame *page = plainPage();

//Create Layout managers
	vlay = new QVBoxLayout( page, 2, 2 );
	hlay = new QHBoxLayout( 2 ); //this mgr will be added to vlay

//Create Widgets
	SearchBox = new QLineEdit( page, "SearchBox" );
	
	filterTypeLabel = new QLabel( page, "filterTypeLabel" );
	filterTypeLabel->setAlignment( AlignRight );
	filterTypeLabel->setText( i18n( "Filter by type: " ) );
	
	filterType = new QComboBox( page, "filterType" );
	filterType->setEditable( false );
	filterType->insertItem( i18n ("Any") );
	filterType->insertItem( i18n ("Constellations") );
	filterType->insertItem( i18n ("Stars") );
	filterType->insertItem( i18n ("Double Stars") );
	filterType->insertItem( i18n ("Planets") );
	filterType->insertItem( i18n ("Open Clusters") );
	filterType->insertItem( i18n ("Glob. Clusters") );
	filterType->insertItem( i18n ("Gas. Nebulae") );
	filterType->insertItem( i18n ("Plan. Nebulae") );
	filterType->insertItem( i18n ("SN Remnants") );
	filterType->insertItem( i18n ("Galaxies") );
	
	SearchList = new QListBox( page, "SearchList" );
	SearchList->setMinimumWidth( 256 );
	SearchList->setMinimumHeight( 320 );
	SearchList->setVScrollBarMode( QListBox::AlwaysOn );
	SearchList->setHScrollBarMode( QListBox::AlwaysOff );

//Pack Widgets into layout manager
	hlay->addWidget( filterTypeLabel, 0, 0 );
	hlay->addWidget( filterType, 0, 0 );
	
	vlay->addWidget( SearchBox, 0, 0 );
	vlay->addSpacing( 12 );
	vlay->addWidget( SearchList, 0, 0 );
	vlay->addLayout( hlay, 0 );
	
	vlay->activate();

// no item currently set
	currentitem = 0;

// no filters set
	Filter = 0;
	
//Connect signals to slots
	connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
	connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
	connect( SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( filter() ) );
	connect( filterType, SIGNAL( activated( int ) ), this, SLOT( setFilter( int ) ) );
	connect (SearchList, SIGNAL (selectionChanged  (QListBoxItem *)), SLOT (updateSelection (QListBoxItem *)));
	
	startTimer (0);		// we use this timer to make sure initObjectList is called only after
                   // the dialog is created and shown.
}

FindDialog::~FindDialog() {
	delete SearchList;
}

void FindDialog::timerEvent (QTimerEvent *)
{
	killTimers(); //make sure timer fires only one time.
	SearchBox->clear();  // QLineEdit
	filterType->setCurrentItem( 0 );  // all types
	filter();
}

void FindDialog::filter() {  //Filter the list of names with the string in the SearchBox
	KStars *p = (KStars *)parent();
	
	int i=0;
	SearchList->clear();
	SearchList->sort (false);		// the Items will faster inserted

	ObjectNameList &ObjNames = p->data()->ObjNames;
	// check if latin names are used
	ObjNames.setLanguage( p->options()->useLatinConstellNames );
	
	QString searchFor = SearchBox->text().lower();
		for ( SkyObjectName *name = ObjNames.first( searchFor ); name; name = ObjNames.next() ) {
			if ( name->text().lower().startsWith( searchFor ) ) {
				new SkyObjectNameListItem ( SearchList, name );
				if ( i++ >= 5000 ) {              //Every 5000 name insertions,
					kapp->processEvents ( 50 );		//spend 50 msec processing KApplication events
					i = 0;
				}
			}
		}
	SearchList->sort (true);  // sort QListBox
	setListItemEnabled(); // Automatically highlight first item
	SearchBox->setFocus();  // set cursor to QLineEdit
}

void FindDialog::filterByType() {
	KStars *p = (KStars *)parent();

	int i = 0;	
	SearchList->clear();	// QListBox
	SearchList->sort (false);		// the Items will faster inserted
	QString searchFor = SearchBox->text().lower();  // search string
	
	ObjectNameList &ObjNames = p->data()->ObjNames;
	// check if latin names are used
	ObjNames.setLanguage( p->options()->useLatinConstellNames );

	for ( SkyObjectName *name = ObjNames.first( searchFor ); name; name = ObjNames.next() ) {
		if ( name->skyObject()->type() + 2 == Filter ) {
			if ( name->text().lower().startsWith( searchFor ) ) {
				new SkyObjectNameListItem ( SearchList, name );
			}
		}
		if (i++ >= 5000) {            //Every 5000 name insertions,
			kapp->processEvents (50);   //spend 50 msec processing KApplication events
			i = 0;
		}
	}

	SearchList->sort (true);  // sort QListBox
	setListItemEnabled();    // Automatically highlight first item
	SearchBox->setFocus();  // set cursor to QLineEdit
}

void FindDialog::setListItemEnabled() {
	SearchList->setSelected (0, true);
	if (!SearchList->isSelected (0))
		updateSelection (0);
}

void FindDialog::updateSelection (QListBoxItem *it) {
	currentitem = (SkyObjectNameListItem *) it;
	SearchBox->setFocus();  // set cursor to QLineEdit
}

void FindDialog::setFilter( int f ) {
	// check if filter was changed or if filter is still the same
	if ( Filter != f ) {
		Filter = f;
		if ( Filter == 0 ) {  // any type will shown
		// delete old connections and create new connections
			disconnect( SearchBox, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterByType() ) );
			connect( SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( filter() ) );
			filter();
		}
		else {
		// delete old connections and create new connections
			disconnect( SearchBox, SIGNAL( textChanged( const QString & ) ), this, SLOT( filter() ) );
			connect( SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( filterByType() ) );
			filterByType();
		}
	}
}

#include "finddialog.moc"
