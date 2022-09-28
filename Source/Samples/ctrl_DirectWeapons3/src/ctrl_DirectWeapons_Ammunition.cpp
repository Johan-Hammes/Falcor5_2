#include "Controllers/ctrl_DirectWeapons/ctrl_DirectWeapons.h"
#include "ew_Scene.h"
#include "server_defines.h"
#include "d3dx9.h"
#include "d3d9.h"
#include <algorithm>
#include "math.h"
#include "earthworks_Raknet.h"
#include "Logger\CSVFile.h"
#include "Util/DialogUtil.h"
#include <stdio.h>



void ctrl_DirectWeapons::ammoChange(DmStandardDropdown& sender, const DmString& value, const DmString& text)
{
	m_activeAmmunition = value.toInt();
	ammosetThis();
}


void ctrl_DirectWeapons::buildGUIammo()
{
	DmWin32* pW32 = m_pScene->getDmWin32();
	DmGui *pGUI = m_pScene->getGUI();
	DmWindow *_pWindow = m_pScene->getGUI_Window();
	if( DmPanel *p3D = dynamic_cast<DmPanel*>(_pWindow->findWidget(L"pnl3D")) )
	{
		m_pAmmo = &p3D->addWidget<DmPanel>( L"Ammo", L"", m_rect_Menu.topLeft.x, m_rect_Menu.topLeft.y, m_rect_Menu.bottomRight.x, m_rect_Menu.bottomRight.y, L"metro_darkblue" );
		m_pAmmo->setClipChildren( false );
		DmGui *pGUI = m_pScene->getGUI();
		wstring guiDir = ((*m_pScene->getConfigManager())[L"Earthworks_3"][L"INSTALL_DIR"]).getWString() + L"/media/gui/Gui/";
		pGUI->loadFragment( *m_pAmmo, guiDir + L"frag_RedI_ammo.xml");

		DmPanel *pP;
		pP = (DmPanel*)m_pAmmo->findWidget( L"left" );					//---------------------------  left arrow
		pP->setCommandText( L"ammo_left" );
		pP->setValue( L"left" );			
		DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomIcon );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );

		pP = (DmPanel*)m_pAmmo->findWidget( L"right" );					//---------------------------  left arrow
		pP->setCommandText( L"ammo_right" );
		pP->setValue( L"right" );			
		DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomIcon );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );

		pP = (DmPanel*)m_pAmmo->findWidget( L"menu" );					//---------------------------  main menu
		pP->setCommandText( L"menu" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

		//pP = (DmPanel*)m_pAmmo->findWidget( L"set_this" );					//---------------------------  set this
		//pP->setCommandText( L"set_this" );
		//DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );

		pP = (DmPanel*)m_pAmmo->findWidget( L"set_all" );					//---------------------------  set all
		pP->setCommandText( L"set_all" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );

		//pP = (DmPanel*)m_pAmmo->findWidget( L"save" );					//---------------------------  save
		//pP->setCommandText( L"set_save" );
		//DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );

		pP = (DmPanel*)m_pAmmo->findWidget( L"boresight" );					//---------------------------  boresight
		pP->setCommandText( L"set_boresight" );
		DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );


		//DmConnect( pP->drawCustomBackground, *this, &ctrl_DirectWeapons::drawCustomIcon );
		//DmConnect( pP->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );

		DmStandardDropdown *pAmmoType = (DmStandardDropdown *)m_pAmmo->findWidget( L"ddlAmmo");
		DmConnect( pAmmoType->selectedItemChange, *this, &ctrl_DirectWeapons::ammoChange );

		scanAmmunition();


		// Die naam kier kant van die skerm -------------------------------------------------------------------------------------------------------
		m_pName_Search = (DmPanel*)m_pAmmo->findWidget( L"names_type" );
		m_pName_Search->setHorizontalTextAlignment( dmAlignLeft );
		m_pName_Search->setHorizontalTextMargin( 20 );

		for (int i=0; i<8; i++)
		{
			m_pName_Results[i] = (DmPanel*)m_pAmmo->findWidget( L"N_"+DmString(i) );
			DmPanel *pH = (DmPanel*)m_pName_Results[i]->findWidget( L"result" );
			pH->setValue( i );
			pH->setCommandText(L"returnName");
			DmConnect( pH->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
		}


		
		m_pAmmo->findWidget( L"boreSight" )->hide();
		((DmPanel*)m_pAmmo->findWidget( L"boreSight" ))->setClipChildren( false );
		((DmPanel*)m_pAmmo->findWidget( L"XXX" ))->setClipChildren( false );
		((DmPanel*)m_pAmmo->findWidget( L"boreSight" ))->setSize( m_rect_3D.width(), m_rect_3D.height() );
		((DmPanel*)m_pAmmo->findWidget( L"boreSight" ))->setPosition( m_rect_3D.topLeft.x - m_rect_Menu.topLeft.x, m_rect_3D.topLeft.y );
		
		DmConnect( ((DmPanel*)m_pAmmo->findWidget( L"boresight_reset" ))->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );
		DmConnect( ((DmPanel*)m_pAmmo->findWidget( L"boresight_clear" ))->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );
		DmConnect( ((DmPanel*)m_pAmmo->findWidget( L"boresight_adjust" ))->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );
		DmConnect( ((DmPanel*)m_pAmmo->findWidget( L"boresight_done" ))->mouseClick, *this, &ctrl_DirectWeapons::toolClickammo );
		((DmPanel*)m_pAmmo->findWidget( L"target" ))->setCommandText( L"mouseShot" );
		DmConnect( ((DmPanel*)m_pAmmo->findWidget( L"target" ))->mouseClick, *this, &ctrl_DirectWeapons::toolClick );

		DmConnect( ((DmPanel*)m_pAmmo->findWidget( L"names_clear" ))->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
		DmConnect( ((DmPanel*)m_pAmmo->findWidget( L"names_rescan" ))->mouseClick, *this, &ctrl_DirectWeapons::toolClick );
	}


	

	m_activeAmmunition  = 0;
	m_activeAmmoLane = 0;
	ammoSetAll();
	
	for (unsigned int i=0; i< m_lanes.size(); i++)
	{
		ewstring lane_str = "lane_" + ewstring( i );
		if (m_pScene->getConfigManager()->hasGroup(lane_str))
		{
			m_activeAmmunition  = (*m_pScene->getConfigManager())[lane_str]["active_ammo"];
			m_activeAmmunition = __min( m_AmmoTypes.size() - 1, __max( 0, m_activeAmmunition ));
			m_activeAmmoLane = i;
			ammosetThis( true );

			

			//m_lanes[i].ammo.ammoIndex = (*m_pScene->getConfigManager())[lane_str]["active_ammo"];
			int ox = (*m_pScene->getConfigManager())[lane_str]["active_ammo_X"];
			int oy = (*m_pScene->getConfigManager())[lane_str]["active_ammo_Y"];
			m_lanes[i].weaponOffset= f_Vector( ox, oy, 0, 0 );
			m_pAmmo->findWidget( L"offset" )->setText( L"(" + DmString((int)m_lanes[m_activeAmmoLane].weaponOffset.x) + L", " + DmString((int)m_lanes[m_activeAmmoLane].weaponOffset.y) + L")" );

			//m_pLogger->Log( LOG_INFO, L"lane %d : %s    \toffsets(%d, %d)",  m_activeAmmoLane, m_AmmoTypes[m_activeAmmunition].name.c_str(), ox, oy);
		}
	}
	m_activeAmmunition  = 0;
	m_activeAmmoLane = 0;
	
}


#define _onCLICK(x,y) if ( cmd == x) { y; return; }

void	ctrl_DirectWeapons::clickProcessAmmo( ewstring cmd, ewstring val, ewstring hlp, ewstring ext )
{
	if ( m_b3DTerminal )
	{
	}

	_onCLICK( "showAmmoScreen",		int L = val.getInt() - m_LaneZero;   
								if ((L>=0) && (L<m_NumLanes)) 
								{
									m_activeAmmoLane = L;	
									m_ammoAnimNewLane = m_activeAmmoLane;		
									populateAmmo( m_activeAmmoLane ); 
									boresightShots.clear();   
								}	);

	_onCLICK( "ammo_left",			m_ammoAnimNewLane = __max(0,						   m_activeAmmoLane - 1 );	  m_animTime = 0;	boresightShots.clear();		displayOffsets() );
	_onCLICK( "ammo_right",		m_ammoAnimNewLane = __min(m_Scenario.get_numLanes()-1, m_activeAmmoLane + 1 );	  m_animTime = 0;	boresightShots.clear();		displayOffsets() );
	//_onCLICK( L"set_this",			ammosetThis();  );
	_onCLICK( "set_all",			ammoSetAll();  );
	_onCLICK( "save",				ammoSave();  );
	_onCLICK( "set_boresight",		m_pAmmo->findWidget( L"boreSight" )->show();  	((DmPanel *)m_pAmmo->findWidget( L"target" ))->clearWidgets();	boresightShots.clear();		m_pScene->setRefFrame( true, 4 ); bBoresight = true;);

	_onCLICK( "boresight_reset",	 ((DmPanel *)m_pAmmo->findWidget( L"target" ))->clearWidgets();		resetOffsets();			boresightShots.clear();	);
	_onCLICK( "boresight_clear",	 ((DmPanel *)m_pAmmo->findWidget( L"target" ))->clearWidgets();		boresightShots.clear();	);
	_onCLICK( "boresight_adjust",	 ((DmPanel *)m_pAmmo->findWidget( L"target" ))->clearWidgets();		adjustBoresight(); );
	_onCLICK( "boresight_done",	 m_pAmmo->findWidget( L"boreSight" )->hide(); );
}


void ctrl_DirectWeapons::toolClickammo(DmPanel& sender, const DmVec2& cursorPosition, DmMouseButton button)
{
	if (!m_bAuthorized) checkLicense();

	if (m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICKAMMO );
		ewstring cmd = sender.getCommandText();
		ewstring val = sender.getValue();
		ewstring hlp = sender.getHelpText();
		Bs.Write( cmd.c_str() );
		Bs.Write( val.c_str() );
		Bs.Write( hlp.c_str() );
		Bs.Write( "" );
		m_pScene->rpc_call( "rpc_CALL", &Bs );
	}

	clickProcessAmmo( ewstring(sender.getCommandText()), ewstring(sender.getValue()), ewstring(sender.getHelpText()), "" );

}

const DmString style1 = L"metro_orange";

// scans teh harddrive for all ammo typs on thsi machign - Buidl a list<>
void ctrl_DirectWeapons::scanAmmunition()
{
	//m_pLogger->Log( LOG_INFO, L"scanAmmunition(..)" );
	m_AmmoTypes.clear();
	wstring test = ((*m_pScene->getConfigManager())[L"Earthworks_3"][L"INSTALL_DIR"]).getWString() + L"/media/";
	wstring ammoDir = test + L"/controllers/DirectWeapons3/ammunition/ammunition.xml";
	_ammo A;

	
	//MySQLManager *mysql = this->getMySQLManager();
	bool mysqlError = true;
	if ( m_pMySQLManager->isConnected() )
	{
		//m_pLogger->Log( LOG_INFO, L"scanAmmunition()... reading from SQL database" );
		string query = "SELECT name, velocity, bc, weight, sight, distance, cyclingType, laserOffset, shotCount, shotSpread, overlay FROM weapontype WHERE companyID = %1q:companyID AND applicationID = (SELECT id FROM applications WHERE name = %2q:appName) ORDER BY pos";
		MySQLInput input;
		input["companyID"] = m_iCompanyID;
		input["appName"] = APP_NAME;
		vector<mysqlpp::Row> result;
		if (m_pMySQLManager->getObjects(query.c_str(), input, &result))
		{
			//m_pLogger->Log( LOG_INFO, L"scanAmmunition()...mysql->getObjects(%d)", result.size() );
			mysqlError = false;

			for (int i = 0; i < result.size(); i++)
			{
				A.name = DmString(result[i]["name"].c_str());
				A.velocity = result[i]["velocity"];
				A.bc = result[i]["bc"];
				A.weight = result[i]["weight"];
				A.sight = result[i]["sight"];
				A.distance = result[i]["distance"];
				//A.type = DmString(result[i]["cyclingType"].c_str());
				DmString type =  DmString(result[i]["cyclingType"].c_str());
				A.type = ammo_STD;
				if (type == L"air")	A.type = ammo_AIRCYCLED;
				A.laser_offset_mm = result[i]["laserOffset"];
				A.shot_count = result[i]["shotCount"];
				A.shot_spread = result[i]["shotSpread"];
				A.overlay = DmString(result[i]["overlay"].c_str());

				m_AmmoTypes.push_back(A);
				//m_pLogger->Log( LOG_INFO, L"scanAmmunition()...%s", A.name.c_str() );
			}
		}
		else
			MessageBoxA(NULL, m_pMySQLManager->getLastError(), "Database Error", MB_OK | MB_ICONERROR);
	}
	else
	{
		TiXmlDocument		document;
		m_pLogger->Log( LOG_INFO, L"scanAmmunition()...from file (%s)", ammoDir.c_str() );
		if ( document.LoadFile( ammoDir.c_str() ) )
		{
			TiXmlNode *ammo = document.FirstChild( L"ammo" );
			TiXmlNode *ammoNode = ammo->FirstChild( L"ammunition" );
			while(ammoNode )
			{
				A.name =  ( ammoNode->ToElement()->Attribute(L"name") );
				A.velocity = _wtof( ammoNode->ToElement()->Attribute(L"velocity") );
				A.bc = _wtof( ammoNode->ToElement()->Attribute(L"bc") );
				A.weight = _wtof( ammoNode->ToElement()->Attribute(L"weight") );
				A.sight = _wtof( ammoNode->ToElement()->Attribute(L"sight") );
				A.distance = _wtof( ammoNode->ToElement()->Attribute(L"distance") );
				DmString type = ( ammoNode->ToElement()->Attribute(L"type") );
				A.type = ammo_STD;
				if (type == L"air")	A.type = ammo_AIRCYCLED;
				A.laser_offset_mm = _wtof( ammoNode->ToElement()->Attribute(L"laser_offset_mm") );
				A.overlay = L"rifle";		// set a default
				A.overlay = ammoNode->ToElement()->Attribute(L"overlay");

				A.shot_count = 1; // for rifle
				A.shot_spread = 0.0; // for rifle
				if (ammoNode->ToElement()->Attribute(L"shot_count"))	A.shot_count = _wtoi( ammoNode->ToElement()->Attribute(L"shot_count"));
				if (ammoNode->ToElement()->Attribute(L"shot_spread"))	A.shot_spread = (float)_wtof( ammoNode->ToElement()->Attribute(L"shot_spread"));

				m_AmmoTypes.push_back( A );
				ammoNode = ammoNode->NextSibling();
			}
		}
	}
			
	

	// Fill the Dropdoen box
	DmStandardDropdown *pAmmoType = (DmStandardDropdown *)m_pAmmo->findWidget( L"ddlAmmo");
	
	pAmmoType->setDropListStyleName( style1 );
	pAmmoType->setDropListItemPanelStyleName(style1);
	pAmmoType->setDropListSelectedItemPanelStyleName(style1);
	pAmmoType->setDropListScrollButtonStyleName(style1);
	DmSelectableItemContainer &CT = pAmmoType->getList();
	CT.clearItems();
	for (int i=0; i<m_AmmoTypes.size(); i++)
	{
		CT.addItem( DmString( i ), m_AmmoTypes[i].name);
	}

}

void ctrl_DirectWeapons::populateAmmo( int lane, bool bShow )
{
	DmPanel *pP;
	if(bShow)	m_pAmmo->show();
	else		m_pAmmo->hide();
	//if (this->m_b3DTerminal) m_pAmmo->hide();


	pP = (DmPanel*)m_pAmmo->findWidget( L"lane" );	
	pP->setText( DmString(lane + 1) );

	pP = (DmPanel*)m_pAmmo->findWidget( L"name" );	
	pP->setText(m_lanes[lane].userNAME );

	pP = (DmPanel*)m_pAmmo->findWidget( L"ammo" );	
	pP->setText(m_lanes[lane].ammo.name );

	// ammunition --------------------------------------------------------------------------------------------------------------------------------------
	pP = (DmPanel*)m_pAmmo->findWidget( L"A" );	
	pP->setText( DmString( m_AmmoTypes[ m_activeAmmunition ].velocity ) );

	pP = (DmPanel*)m_pAmmo->findWidget( L"B" );	
	pP->setText( DmString( m_AmmoTypes[ m_activeAmmunition ].bc, 3 ) );

	pP = (DmPanel*)m_pAmmo->findWidget( L"C" );	
	pP->setText( DmString( m_AmmoTypes[ m_activeAmmunition ].weight ) );

	pP = (DmPanel*)m_pAmmo->findWidget( L"D" );	
	pP->setText( DmString( m_AmmoTypes[ m_activeAmmunition ].sight, 2 ) );

	pP = (DmPanel*)m_pAmmo->findWidget( L"E" );	
	pP->setText( DmString( m_AmmoTypes[ m_activeAmmunition ].distance ) );

	pP = (DmPanel*)m_pAmmo->findWidget( L"F" );	



	// dateer oo die hoof menu met die inligtign op
	//if ( bShow) m_pLogger->Log( LOG_INFO, L"populateAmmo lane(%d), %s, %s   SHOW",  lane, m_lanes[lane].userNAME.c_str(), m_lanes[lane].ammo.name.c_str() );
	//else		m_pLogger->Log( LOG_INFO, L"populateAmmo lane(%d), %s, %s   HIDE",  lane, m_lanes[lane].userNAME.c_str(), m_lanes[lane].ammo.name.c_str() );

	m_pMenu->findWidget( L"LaneName"+DmString(lane) )->setText(m_lanes[lane].userNAME);	
	m_pMenu->findWidget( L"LaneAmmo"+DmString(lane) )->setText(m_lanes[lane].ammo.name);	


	// now updatethe dropdown list if ts different --------------------------------------------------------
	m_activeAmmunition = m_lanes[m_activeAmmoLane].ammo.ammoIndex;
	DmStandardDropdown *pAmmoType = (DmStandardDropdown *)m_pAmmo->findWidget( L"ddlAmmo");
	if( pAmmoType->getList().getSelectedIndex() != m_activeAmmunition )
	{
		pAmmoType->getList().setSelectedIndex( m_activeAmmunition );
	}

	if (m_bInstructorStation)
	{
		RakNet::BitStream Bs;
		Bs.Write( (unsigned short)RPC_TOOLCLICK );
		if (bShow)	Bs.Write( "TERMINAL_assign_name_SHOW" );
		else		Bs.Write( "TERMINAL_assign_name_HIDE" );
		Bs.Write( ewstring(lane).c_str() );
		Bs.Write( ewstring(m_lanes[lane].userNAME).c_str() );
		Bs.Write( ewstring(m_activeAmmunition).c_str() );
		m_pScene->rpc_call( "rpc_CALL", &Bs );
	}

	
}


void ctrl_DirectWeapons::displayOffsets()
{
	m_pAmmo->findWidget( L"offset" )->setText( L"(" + DmString((int)m_lanes[m_activeAmmoLane].weaponOffset.x) + L", " + DmString((int)m_lanes[m_activeAmmoLane].weaponOffset.y) + L")" );
}

void ctrl_DirectWeapons::resetOffsets()
{
	if (m_bLiveFire)	m_lanes[m_activeAmmoLane].weaponOffset = f_Vector( 0, m_lanes[m_activeAmmoLane].ammo.m_live_offset_pixels, 0, 0);
	else				m_lanes[m_activeAmmoLane].weaponOffset = f_Vector( 0, m_lanes[m_activeAmmoLane].ammo.m_laser_offset_pixels, 0, 0);
	m_pAmmo->findWidget( L"offset" )->setText( L"(" + DmString((int)m_lanes[m_activeAmmoLane].weaponOffset.x) + L", " + DmString((int)m_lanes[m_activeAmmoLane].weaponOffset.y) + L")" );
}


void  ctrl_DirectWeapons::adjustBoresight()
{
	if(boresightShots.size() == 0 ) return;

	f_Vector avs(0, 0, 0, 0 );
	for (int i=0; i<boresightShots.size(); i++)
	{
		avs = avs + boresightShots[i];
	}
	avs = avs * (1.0 /  (float)boresightShots.size());
	m_lanes[m_activeAmmoLane].weaponOffset = m_lanes[m_activeAmmoLane].weaponOffset + avs;

	m_pAmmo->findWidget( L"offset" )->setText( L"(" + DmString((int)m_lanes[m_activeAmmoLane].weaponOffset.x) + L", " + DmString((int)m_lanes[m_activeAmmoLane].weaponOffset.y) + L")" );

	boresightShots.clear();

}


void ctrl_DirectWeapons::ammosetThis( bool bupdateGUI )
{
	_ammo A = m_AmmoTypes[m_activeAmmunition];	//?? wil ek dit he of direk van die GUI af teruglees?
	A.ammoIndex = m_activeAmmunition;
	ammoSolveBallistics( &A );

	m_lanes[m_activeAmmoLane].ammo = A;

	//if (bupdateGUI)
	{
		populateAmmo( m_activeAmmoLane, bupdateGUI );
		//resetOffsets();				// FIXME die moet verdwyn as ek eers alal eammo se offset sstoor
	}
}


void ctrl_DirectWeapons::ammoSetAll()
{
	_ammo A = m_AmmoTypes[m_activeAmmunition];	//?? wil ek dit he of direk van die GUI af teruglees?
	A.ammoIndex = m_activeAmmunition;
	ammoSolveBallistics( &A );

	for (int i=0; i<m_lanes.size(); i++)
	{
		m_lanes[i].ammo = A;
		populateAmmo( i );
	}

	populateAmmo( m_activeAmmoLane, !m_b3DTerminal );
}


void ctrl_DirectWeapons::ammoSave()
{
}


void ctrl_DirectWeapons::ammoBoresight()
{
}


void ctrl_DirectWeapons::ammoSolveBallistics( _ammo *pAmmo )
{
	float T_BulletDrop[500];
	float T_time[500];
	float T_Velocity[500];

	double temperature = 21 + 273.15;			// kelvin
	float altitude = 0;				// meters above sea level
	float alt_Scale = exp( -(altitude) / 8000 );
	float temp_Scale = 360.77869 * pow(temperature, -1.00336) / ( 360.77869 * pow(21 + 273.15, -1.00336) );

	f_Vector g(0, -9.80665, 0, 0);
	float c_G1 = 0.5191;			// Cd of G1
	float c_Ft_M = 0.30480;			// feet to meters
	float c_lbin_kgm = 703.06;		// poundpersquareinch to km/squaremeter
	float c_Gr_Kg = 0.000064799;	// grains to kg
	float inch_m =	0.0254;			//1 Inch = 0.0254 Meters

	pAmmo->m_muzzleVelocity = pAmmo->velocity * c_Ft_M;
	pAmmo->m_Weight = pAmmo->weight * c_Gr_Kg;
	pAmmo->m_Cd = pAmmo->m_Weight * c_G1 / pAmmo->bc / 1.27324 / c_lbin_kgm;

	// First solve for bullet drop - out to 500 meters -----------------------------------------------------------------------------------------------------
	float time=0, V, s_drag;
	f_Vector pos(0, 0, 0, 0);
	f_Vector velocity(pAmmo->m_muzzleVelocity, 0, 0, 0);
	f_Vector drag;
	while ((pos.x < 400 ) )
	{
		V = velocity.length();
		s_drag = -0.5 * V * V * pAmmo->m_Cd * alt_Scale * temp_Scale * temp_Scale;
		drag = velocity;
		drag.normalize();
		velocity = velocity + ((g + (drag * s_drag)*(1.0f/pAmmo->m_Weight)) * 0.0001);
		pos = pos + (velocity * 0.0001);
		time += 0.0001;

		T_BulletDrop[(int)pos.x] = pos.y;
		T_time[(int)pos.x] = time;
		T_Velocity[(int)pos.x] = velocity.length();
	}

	// Calculate offsets and repeat a second pass with shoot in angle included ------------------------------------------------------------------------------
	float drop = T_BulletDrop[ pAmmo->distance -1 ]; // want on het eintlik die afstande aand ie einde van die bucket hier in
	pAmmo->m_barrelDrop = pAmmo->sight * inch_m;
	pAmmo->m_barrelRotation = atan2( pAmmo->m_barrelDrop - drop, pAmmo->distance );
	pAmmo->m_barrelRotation *= cos(pAmmo->m_barrelRotation);
	velocity = f_Vector( pAmmo->m_muzzleVelocity * cos( pAmmo->m_barrelRotation ), pAmmo->m_muzzleVelocity * sin( pAmmo->m_barrelRotation ), 0, 0 );
	pos = d_Vector(0, -pAmmo->m_barrelDrop, 0);
	time = 0.0;
	while ((pos.x < 499 ) )
	{
		drag = velocity-f_Vector( 0, 0, 1 );		// 1m/s wind tyo the right
		V = drag.length();		
		s_drag = -0.5 * V * V * pAmmo->m_Cd * alt_Scale * temp_Scale * temp_Scale;
		drag.normalize();
		velocity = velocity + ((g + (drag * s_drag)*(1.0f/pAmmo->m_Weight)) * 0.0001f);
		pos = pos + (velocity * 0.0001);
		time += 0.0001;

		T_BulletDrop[(int)pos.x] = pos.y;
		T_time[(int)pos.x] = time;
		T_Velocity[(int)pos.x] = velocity.length();
		pAmmo->m_bulletDrop[(int)pos.x] = pos.y;
		pAmmo->m_bulletDrift[(int)pos.x] = pos.z;
	}

	// baan inligting ---------------------------------------------------------------------------------------------------------------------------------------
	float W = m_pScreenSetup->findWidget( L"SS_width" )->getText().toFloat() / 1000.0f;
	float D = m_pScreenSetup->findWidget( L"SS_distance" )->getText().toFloat() / 1000.0f;
	float fov = 2.0 * atan2(W/2, D) * 57;
	
	//FIXME hierdie is baie onakuraat as ons vreeslik naby aan die skerm staan -  dan moet mens mooier werk
	float skerm_afstand = D - 1;	// die -1 is omdat dit van die voorkat van die loop af is	
	pAmmo->m_live_offset = T_BulletDrop[ (int)skerm_afstand ];
	pAmmo->m_laser_offset = tan( pAmmo->m_barrelRotation )*skerm_afstand - pAmmo->m_barrelDrop + (pAmmo->laser_offset_mm/1000.0f);

	float resolution = m_rect_3D.width();	// wydte in pixels
	float m_to_pixel = resolution / W;		// aanvaar 1:1 pixels
	pAmmo->m_live_offset_pixels = pAmmo->m_live_offset * m_to_pixel;
	pAmmo->m_laser_offset_pixels = pAmmo->m_laser_offset * m_to_pixel;

	// dump ammo report to disk ------------------------------------------------------------------------------------------------
	FILE *pFile = fopen( "ammo.txt", "w" );
		fwprintf( pFile, L"%s\n\n", pAmmo->name.c_str() );
		fprintf( pFile, "Vel  - %d ft/s,	%d m/s\n", (int)pAmmo->velocity, (int)pAmmo->m_muzzleVelocity );
		fprintf( pFile, "BC   - %3.3f,		Cd - %3.7f\n", (float)pAmmo->bc, (float)pAmmo->m_Cd );
		fprintf( pFile, "Wgt  - %d grains,	%3.2f g\n", (int)pAmmo->weight, (float)pAmmo->m_Weight*1000 );
		fprintf( pFile, "Sgt  - %3.2f inch,	%3.3f mm\n", (float)pAmmo->sight, pAmmo->m_barrelDrop * 1000 );
		fprintf( pFile, "Dst  - %d m\n\n", (int)pAmmo->distance );

		int i=0;
		fprintf( pFile, "Start           - %8dm, %8dmm, %8.2fs \n", i, (int)(-pAmmo->m_barrelDrop * 1000), T_time[i] );

		for (;i<395; i++)
		{
			if (i==skerm_afstand+1)																	fprintf( pFile, "Screen          - %8dm, %8dmm, %8.2fs \n", i, (int)(T_BulletDrop[i-1]*1000), T_time[i-1] );
			if (i==50)																				fprintf( pFile, "50m             - %8dm, %8dmm, %8.2fs \n", i, (int)(T_BulletDrop[i]*1000), T_time[i] );
			if (i==100)																				fprintf( pFile, "100m            - %8dm, %8dmm, %8.2fs \n", i, (int)(T_BulletDrop[i]*1000), T_time[i] );
			if (i==150)																				fprintf( pFile, "150m            - %8dm, %8dmm, %8.2fs \n", i, (int)(T_BulletDrop[i]*1000), T_time[i] );
			if (i==200)																				fprintf( pFile, "200m            - %8dm, %8dmm, %8.2fs \n", i, (int)(T_BulletDrop[i]*1000), T_time[i] );
			if (i==250)																				fprintf( pFile, "250m            - %8dm, %8dmm, %8.2fs \n", i, (int)(T_BulletDrop[i]*1000), T_time[i] );
			if (i==300)																				fprintf( pFile, "300m            - %8dm, %8dmm, %8.2fs \n", i, (int)(T_BulletDrop[i]*1000), T_time[i] );
			if ( i>0 )
			{
				if( T_BulletDrop[i]>=0 && T_BulletDrop[i-1]<0 )											fprintf( pFile, "First Crossing  - %8dm, %8dmm, %8.2fs \n", i, (int)(T_BulletDrop[i]*1000), T_time[i] );
				if( (T_BulletDrop[i]-T_BulletDrop[i-1])>=0 && (T_BulletDrop[i+1]-T_BulletDrop[i])<0 )	fprintf( pFile, "Apex            - %8dm, %8dmm, %8.2fs \n", i, (int)(T_BulletDrop[i]*1000), T_time[i] );
				if( T_BulletDrop[i]<=0 && T_BulletDrop[i-1]>0 )											fprintf( pFile, "Second Crossing - %8dm, %8dmm, %8.2fs \n", i, (int)(T_BulletDrop[i]*1000), T_time[i] );
			}
		}
		fprintf( pFile, "\n");
		fprintf( pFile, "Screen  - width - %8.3fm, distance - %8.3fm, fov - %3.1f\n", W, D, fov );
		fprintf( pFile, "Screen  - resolution - %d pixels, %3.2f pixels/m \n\n", (int)resolution, m_to_pixel );

		fprintf( pFile, "Laser offset  - %3.1f mm, %3.1f pixels \n", pAmmo->m_laser_offset*1000,  pAmmo->m_laser_offset_pixels );
		fprintf( pFile, "Live offset   - %3.1f mm, %3.1f pixels \n", pAmmo->m_live_offset*1000,  pAmmo->m_live_offset_pixels );
	fclose(pFile);
}






void ctrl_DirectWeapons::fireBoresight(float x, float y)
{
	fireTestRoundonBoresight(x, y);
	zigbeeFire( m_activeAmmoLane );

	// add offsets in -------------------------------------------------------------------------------------------------------------------
	x += m_lanes[m_activeAmmoLane].weaponOffset.x;
	y += m_lanes[m_activeAmmoLane].weaponOffset.y;

	// vector----------------------------------------------------------------------------------------------------------------------------
	//d_Vector dir, orig;
	//m_pScene->calcRayFromClick( x, y, &dir, &orig );

	// sounds particles flash------------------------------------------------------------------------------------------------------------
//	SoundSource *m_pSoundSource = SoundManager::GetSingleton()->createSoundSourceFromFile( L"Beretta_shot.wav" );
//	if (m_pSoundSource)
//	{
//		m_pSoundSource->setPosition( orig );
//		m_pSoundSource->setReferenceDistance( 140 );
//		m_pSoundSource->play();
//	}

	// Apply barrel offsets and rotations -----------------------------------------------------------------------------------------------
	//d_Vector U(0, 1, 0, 0); // FIXME ook verkeerd - dis vir spheries
	//U.normalize();
	//orig = orig + U * m_lanes[m_activeAmmoLane].ammo.m_barrelDrop;		// move round 5.7 cm downwards
	//dir = dir + U * m_lanes[m_activeAmmoLane].ammo.m_barrelRotation;	// rotate barrel upwards
	//dir.normalize();

//	rpc_Fire( orig, dir, ,  m_lanes[m_activeAmmoLane].ammo.m_Cd, m_lanes[m_activeAmmoLane].ammo.m_Weight );
	double temperature = 21 + 273.15;			// kelvin
	float altitude = 0;				// meters above sea level
	float alt_Scale = exp( -(altitude) / 8000 );
	float temp_Scale = 360.77869 * pow(temperature, -1.00336) / ( 360.77869 * pow(21 + 273.15, -1.00336) );

	f_Vector g(0, -9.80665, 0, 0);
	float c_G1 = 0.5191;			// Cd of G1
	float c_Ft_M = 0.30480;			// feet to meters
	float c_lbin_kgm = 703.06;		// poundpersquareinch to km/squaremeter
	float c_Gr_Kg = 0.000064799;	// grains to kg
	float inch_m =	0.0254;			//1 Inch = 0.0254 Meters

	// First solve for bullet drop - out to 500 meters -----------------------------------------------------------------------------------------------------
	float time=0, V, s_drag;
	f_Vector velocity = f_Vector( m_lanes[m_activeAmmoLane].ammo.m_muzzleVelocity * cos( m_lanes[m_activeAmmoLane].ammo.m_barrelRotation ), m_lanes[m_activeAmmoLane].ammo.m_muzzleVelocity * sin( m_lanes[m_activeAmmoLane].ammo.m_barrelRotation ), 0, 0 );
	f_Vector pos(0, -m_lanes[m_activeAmmoLane].ammo.m_barrelDrop, 0);
	f_Vector drag;
	while ((pos.x < m_lanes[m_activeAmmoLane].ammo.distance ) )
	{
		V = velocity.length();
		s_drag = -0.5 * V * V *  m_lanes[m_activeAmmoLane].ammo.m_Cd * alt_Scale * temp_Scale * temp_Scale;
		drag = velocity;
		drag.normalize();
		velocity = velocity + ((g + (drag * s_drag)*(1.0f/ m_lanes[m_activeAmmoLane].ammo.m_Weight)) * 0.0001);
		pos = pos + (velocity * 0.0001);
		time += 0.0001;
	}

	//D3DXVECTOR3 scrn = m_pScene->projectToScreen( pos );
	DmPanel *pTarget = (DmPanel  *)m_pAmmo->findWidget( L"target" );
	DmRect2D R = pTarget->getScreenRectangle();
	int tX = x - R.topLeft.x;
	int tY = y - R.topLeft.y;
	static int CNT = 0;
	CNT ++;
	pTarget->addWidget<DmPanel>( L"B1" + DmString(CNT), L"", tX-15, tY-15, tX+15, tY+15, L"bulletX" );


	int bsX = x - R.midX();
	int bsY = y - R.midY();
	boresightShots.push_back( f_Vector(-bsX, -bsY, 0, 0) );
}




void ctrl_DirectWeapons::fireTestRoundonBoresight(float x, float y)
{
	// add offsets in -------------------------------------------------------------------------------------------------------------------
	x += m_lanes[m_activeAmmoLane].weaponOffset.x;
	y += m_lanes[m_activeAmmoLane].weaponOffset.y;

	// vector----------------------------------------------------------------------------------------------------------------------------
	d_Vector AIM_dir, AIM_orig;
	m_pScene->calcRayFromClick( x, y, &AIM_dir, &AIM_orig );
	d_Vector dir, orig;
	m_pScene->calcRayFromClick( x, y, &dir, &orig );

	// Apply barrel offsets and rotations -----------------------------------------------------------------------------------------------
	d_Vector U(0, 1, 0, 0); // FIXME ook verkeerd - dis vir spheries
	U.normalize();
	orig = orig - U * m_lanes[m_activeAmmoLane].ammo.m_barrelDrop;		// move round 5.7 cm downwards
	dir = dir + U * m_lanes[m_activeAmmoLane].ammo.m_barrelRotation;	// rotate barrel upwards
	dir.normalize();

	double temperature = 21 + 273.15;			// kelvin
	float altitude = 0;				// meters above sea level
	float alt_Scale = exp( -(altitude) / 8000 );
	float temp_Scale = 360.77869 * pow(temperature, -1.00336) / ( 360.77869 * pow(21 + 273.15, -1.00336) );

	d_Vector g(0, -9.80665, 0, 0);
	float c_G1 = 0.5191;			// Cd of G1
	float c_Ft_M = 0.30480;			// feet to meters
	float c_lbin_kgm = 703.06;		// poundpersquareinch to km/squaremeter
	float c_Gr_Kg = 0.000064799;	// grains to kg
	float inch_m =	0.0254;			//1 Inch = 0.0254 Meters

	FILE *file = fopen("ballistics.txt", "w");
	fwprintf( file, L"%s\n", m_lanes[m_activeAmmoLane].ammo.name.c_str() );
	fprintf(file, "orig: %2.5f, %2.5f, %2.5f \n", AIM_orig.x, AIM_orig.y, AIM_orig.z );
	fprintf(file, " dir: %2.5f, %2.5f, %2.5f \n ", AIM_dir.x, AIM_dir.y, AIM_dir.z );
	fprintf(file, "\n ");
	fprintf(file, "barrelDrop: %f\n ", m_lanes[m_activeAmmoLane].ammo.m_barrelDrop );
	fprintf(file, "barrelRotation: %f\n ", m_lanes[m_activeAmmoLane].ammo.m_barrelRotation );
	fprintf(file, "orig: %2.5f, %2.5f, %2.5f \n", orig.x, orig.y, orig.z );
	fprintf(file, " dir: %2.5f, %2.5f, %2.5f \n ", dir.x, dir.y, dir.z );
	fprintf(file, "\n ");

	// Solve path out to 500 meters -----------------------------------------------------------------------------------------------------
	double time=0, V, s_drag;
	d_Vector velocity = dir * m_lanes[m_activeAmmoLane].ammo.m_muzzleVelocity;
	d_Vector pos = orig;
	d_Vector drag;
	d_Vector aimPos;
	while ((pos-orig).length() < 300 )
	{
		V = velocity.length();
		s_drag = -0.5 * V * V *  m_lanes[m_activeAmmoLane].ammo.m_Cd * alt_Scale * temp_Scale * temp_Scale;
		drag = velocity;
		drag.normalize();
		velocity = velocity + ((g + (drag * s_drag)*(1.0f/ m_lanes[m_activeAmmoLane].ammo.m_Weight)) * 0.0001);
		pos = pos + (velocity * 0.0001);
		double distance = (pos-orig).length();
		aimPos = AIM_orig + AIM_dir * distance;
		time += 0.0001;
		fprintf(file, "%2.2fm, %2.3fmm  (%2.3f, %2.3f)\n ", distance, 1000*(pos.y - aimPos.y), pos.y, aimPos.y );
	}

	fclose( file );
}



void ctrl_DirectWeapons::animateAmmo( double dTime )
{
	
	if ( m_activeAmmoLane != m_ammoAnimNewLane )
	{
		float dir = m_activeAmmoLane - m_ammoAnimNewLane;
		m_animTime += dTime;
		if (m_animTime<1)
		{
			((DmPanel*)m_pAmmo->findWidget( L"XXX" ))->setPosition( m_animTime*m_rect_Menu.width()*dir, 0 );
		}
		else
		{
			((DmPanel*)m_pAmmo->findWidget( L"XXX" ))->setPosition( 0, 0 );
			m_activeAmmoLane = m_ammoAnimNewLane;
			populateAmmo( m_activeAmmoLane ); 
		}
	}

		
}
