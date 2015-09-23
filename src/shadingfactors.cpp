
#include <wx/clipbrd.h>
#include <wx/dcbuffer.h>
#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/statline.h>
#include <wx/tokenzr.h>
#include <wx/textfile.h>

#include <fstream>

#include <wex/csv.h>

#include "main.h"
#include "widgets.h"

#include "variables.h"
#include "shadingfactors.h"

/*********  SHADING BUTTON CTRL ************/

ShadingInputData::ShadingInputData()
{
	clear();
}

void ShadingInputData::save( std::vector<float> &data )
{
	data.clear();
	data.push_back(3.0); // version number of data format - allows for expansion of options in future.
//	data.push_back(2.0); // version number of data format - allows for expansion of options in future.

//	data.push_back( (en_hourly && hourly.size() == 8760) ? 1.0 : 0.0 );
	data.push_back((en_mxh && mxh.nrows() == 12 && mxh.ncols() == 24) ? 1.0 : 0.0);
	data.push_back( en_azal ? 1.0 : 0.0 );
	data.push_back( en_diff ? 1.0 : 0.0 );
	data.push_back( -1.0 );
	data.push_back( -1.0 );
	data.push_back( -1.0 );

//	for (int i=0;i<8760;i++)
//		data.push_back( i < hourly.size() ? hourly[i] : 0.0 );


	if ( mxh.nrows() != 12 || mxh.ncols() != 24 )
		mxh.resize_fill(12, 24, 0.0);

	for (int r=0;r<12;r++)
		for (int c=0;c<24;c++)
			data.push_back( mxh.at(r,c) );
	
	data.push_back( azal.nrows() );
	data.push_back( azal.ncols() );
	for (int r=0;r<azal.nrows();r++)
		for (int c=0;c<azal.ncols();c++)
			data.push_back( azal.at(r,c) );
	
	data.push_back( diff );

	// timestep at end for potential backwards compatibility
	// timestep string shading fractions matrix added timestep x number of parallel strings
	data.push_back((en_timestep && (timestep.nrows() % 8760 == 0)) ? 1.0 : 0.0);
	data.push_back(timestep.nrows());
	data.push_back(timestep.ncols());
	for (size_t r = 0; r < timestep.nrows(); r++)
		for (size_t c = 0; c < timestep.ncols(); c++)
			data.push_back(timestep.at(r, c));


	data.push_back( data.size() + 1 ); // verification flag that size is consistent
}

void ShadingInputData::clear()
{
//	en_hourly = en_mxh = en_azal = en_diff = en_timestep = false;
	en_mxh = en_azal = en_diff = en_timestep = false;

	timestep.resize(8760, 0);

//	hourly.resize(8760, 0);

	mxh.resize_fill(12,24, 0.0);

	azal.resize_fill(10, 18, 0.0);

	for ( int c=0;c<18;c++ )
		azal.at(0, c) = c*20;
	for ( int r=0;r<10;r++ )
		azal.at(r, 0) = r*10;
	
	diff = 0.0;
}

bool ShadingInputData::load( const std::vector<float> &data )
{
	clear();

	if (data.size() < 3) return false;
	if (data.size() != (int)data[ data.size() - 1 ]) return false; // verification check
	
	int idx = 0; // indexer to step through data

	int ver = (int)data[idx++];	
	if (ver == 2)
	{
		//		en_hourly = data[idx++] > 0 ? true : false;
		en_timestep = data[idx++] > 0 ? true : false;
		en_mxh = data[idx++] > 0 ? true : false;
		en_azal = data[idx++] > 0 ? true : false;
		en_diff = data[idx++] > 0 ? true : false;
		idx++; // skip unused -1
		idx++; // skip unused -1
		idx++; // skip unused -1

//		hourly.clear();
//		hourly.reserve(8760);
//		for (int i=0;i<8760;i++)
//			hourly.push_back( data[idx++] );
		timestep.resize_fill(8760, 1, 0);
		for (int r = 0; r<8760; r++)
			timestep.at(r, 0) = data[idx++];


		for (int r=0;r<12;r++)
			for (int c=0;c<24;c++)
				mxh.at(r,c) = data[idx++];		

		int nr = (int)data[idx++];
		int nc = (int)data[idx++];
		azal.resize_fill( nr, nc, 1.0 );
		for (int r=0;r<nr;r++)
			for (int c=0;c<nc;c++)
				azal.at(r,c) = data[idx++];
		
		diff = data[idx++];
		
		int verify = data[idx++];

		return idx == verify;
	}
	else if (ver == 3)
	{
		en_mxh = data[idx++] > 0 ? true : false;
		en_azal = data[idx++] > 0 ? true : false;
		en_diff = data[idx++] > 0 ? true : false;
		idx++; // skip unused -1
		idx++; // skip unused -1
		idx++; // skip unused -1


		for (int r = 0; r<12; r++)
			for (int c = 0; c<24; c++)
				mxh.at(r, c) = data[idx++];

		int nr = (int)data[idx++];
		int nc = (int)data[idx++];
		azal.resize_fill(nr, nc, 1.0);
		for (int r = 0; r<nr; r++)
			for (int c = 0; c<nc; c++)
				azal.at(r, c) = data[idx++];

		diff = data[idx++];

		en_timestep = data[idx++] > 0 ? true : false;
		nr = (int)data[idx++];
		nc = (int)data[idx++];
		timestep.resize_fill(nr, nc, 0);
		for (int r = 0; r<nr; r++)
			for (int c = 0; c<nc; c++)
				timestep.at(r, c) = data[idx++];


		int verify = data[idx++];

		return idx == verify;
	}


	return false;
}

void ShadingInputData::write( VarValue *vv )
{
	vv->SetType( VV_TABLE );
	VarTable &tab = vv->Table();
	// Version 2 - should be upgraded in project upgrader
	//	tab.Set( "en_hourly", VarValue( (bool)en_hourly ) );
	//	tab.Set( "hourly", VarValue( hourly ) );
	tab.Set( "en_timestep", VarValue( (bool)en_timestep ) );
	tab.Set( "timestep", VarValue( timestep ) );

	tab.Set("en_mxh", VarValue((bool)en_mxh));
	tab.Set( "mxh", VarValue( mxh ) );
	tab.Set( "en_azal", VarValue( (bool)en_azal ) );
	tab.Set( "azal", VarValue( azal ) );
	tab.Set( "en_diff", VarValue( (bool)en_diff ) );
	tab.Set( "diff", VarValue( (float)diff ) );
}

bool ShadingInputData::read( VarValue *root )
{
	clear();
	if ( root->Type() == VV_TABLE )
	{
		VarTable &tab = root->Table();
		// version 2 - should be upgraded in project upgrader
		//if ( VarValue *vv = tab.Get( "en_hourly" ) ) en_hourly = vv->Boolean();
		//if ( VarValue *vv = tab.Get("hourly") ) hourly = vv->Array();
		if ( VarValue *vv = tab.Get( "en_timestep" ) ) en_timestep = vv->Boolean();
		if ( VarValue *vv = tab.Get("timestep") ) timestep = vv->Matrix();
		if (VarValue *vv = tab.Get("en_mxh")) en_mxh = vv->Boolean();
		if ( VarValue *vv = tab.Get("mxh") ) mxh = vv->Matrix();
		if ( VarValue *vv = tab.Get("en_azal") ) en_azal = vv->Boolean();
		if ( VarValue *vv = tab.Get("azal") ) azal = vv->Matrix();
		if ( VarValue *vv = tab.Get("en_diff") ) en_diff = vv->Boolean();
		if ( VarValue *vv = tab.Get("diff") ) diff = vv->Value();
		return true;
	}
	else
		return false;
}


static const char *hourly_text = "The timestep option is appropriate if you have a set of beam shading losses for each of the simulation timesteps in a single year. ";
static const char *mxh_text = "The Month by Hour option allows you to specify a set of 288 (12 months x 24 hours) beam shading losses that apply to the 24 hours of the day for each month of the year. Select a cell or group of cells and type a number between 0% and 100% to assign values to the table by hand. Click Import to import a table of values from a properly formatted text file. ";
static const char *azal_text = "The Azimuth by Altitude option allows you to specify a set of beam shading losses for different sun positions.\n"
  "1. Define the size of the table by entering values for the number of rows and columns.\n"
  "2. Enter solar azimuth values from 0 to 360 degrees in the first row of the table, where 0 = north, 90 = east, 180 = south, and 270 = west.\n"
  "3. Enter solar altitude values from 0 to 90 degrees in the first column of the table, where zero is on the horizon.\n"
  "4. Enter shading losses as the shaded percentage of the beam component of the incident radiation in the remaining table cells.\n"
  "Click Paste to populate the table from your computer\'s clipboard, or click Import to import a table of values from a properly formatted text file.  ";
static const char *diff_text = "The constant sky diffuse shading loss reduces the overall diffuse irradiance available by the specified loss.  Valid values are between 0% and 100%.";

enum { ID_ENABLE_HOURLY = ::wxID_HIGHEST+999,
	ID_ENABLE_MXH, ID_ENABLE_AZAL, ID_ENABLE_DIFF,
	ID_IMPORT_PVSYST_NEAR_SHADING,
	ID_IMPORT_SUNEYE_HOURLY,
	ID_IMPORT_SUNEYE_OBSTRUCTIONS,
	ID_IMPORT_SOLPATH_MXH};

class ShadingDialog : public wxDialog
{
	wxScrolledWindow *m_scrollWin;

	wxCheckBox *m_enableTimestep;
	wxShadingFactorsCtrl *m_timestep;
	wxStaticText *m_textHourly;
	 
	wxCheckBox *m_enableMxH;
	AFMonthByHourFactorCtrl *m_mxh;
	wxStaticText *m_textMxH;

	wxCheckBox *m_enableAzal;
	AFDataMatrixCtrl *m_azal;
	wxStaticText *m_textAzal;

	wxCheckBox *m_enableDiffuse;
	wxNumericCtrl *m_diffuseFrac;
	wxStaticText *m_textDiffuse;

public:
	ShadingDialog( wxWindow *parent, const wxString &descText )
		: wxDialog( parent, wxID_ANY, 
			wxString("Edit shading data") + wxString( (!descText.IsEmpty() ? ": " : "") ) + descText, 
			wxDefaultPosition, wxDefaultSize, 
			wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
	{
		SetEscapeId( wxID_CANCEL );

		SetClientSize( 870, 600 );

		m_scrollWin = new wxScrolledWindow( this, wxID_ANY );
		m_scrollWin->SetScrollRate( 50, 50 );

		m_enableTimestep = new wxCheckBox( m_scrollWin, ID_ENABLE_HOURLY, "Enable timestep beam irradiance shading losses" );
		m_timestep = new wxShadingFactorsCtrl(m_scrollWin, wxID_ANY);
		m_timestep->SetInitialSize(wxSize(900, 200));
		m_timestep->SetMinuteCaption("Time step in minutes:");
		m_timestep->SetColCaption(wxString("Number of parallel strings:") + wxString((!descText.IsEmpty() ? " for " : "")) + descText);
		int num_cols = 8;
		// TODO: 8760 should be weather file timestep
		matrix_t<float> ts_data(8760, num_cols, 0);
		m_timestep->SetData(ts_data);
		wxArrayString cl;
		for (size_t i = 0; i < num_cols; i++)
			cl.push_back(wxString::Format("String %d", i+1));
		m_timestep->SetColLabels(cl);

		m_enableMxH = new wxCheckBox( m_scrollWin, ID_ENABLE_MXH, "Enable month by hour beam irradiance shading losses" );
		m_mxh = new AFMonthByHourFactorCtrl( m_scrollWin, wxID_ANY );
		m_mxh->SetInitialSize( wxSize(900,330) );

		m_enableAzal = new wxCheckBox( m_scrollWin, ID_ENABLE_AZAL, "Enable solar azimuth by altitude beam irradiance shading loss table" );
		m_azal = new AFDataMatrixCtrl( m_scrollWin, wxID_ANY );
		m_azal->SetInitialSize( wxSize(900,280) );
		m_azal->ShowLabels( false );

		matrix_t<float> data(10, 18, 0);
		for ( int c=0;c<18;c++ )
			data.at(0, c) = c*20;
		for ( int r=0;r<10;r++ )
			data.at(r, 0) = r*10;
		m_azal->SetData( data );
		
		m_enableDiffuse = new wxCheckBox( m_scrollWin, ID_ENABLE_DIFF, "Enable sky diffuse shading loss (constant)" );
		m_diffuseFrac = new wxNumericCtrl( m_scrollWin, wxID_ANY, 0.0 );
		
		wxSizer *import_tools = new wxStaticBoxSizer( wxHORIZONTAL, m_scrollWin, "Import shading data from external tools");
		import_tools->Add( new wxButton( m_scrollWin, ID_IMPORT_PVSYST_NEAR_SHADING, "PVsyst near shading..." ), 0, wxALL, 3 );
		import_tools->Add( new wxButton( m_scrollWin, ID_IMPORT_SUNEYE_HOURLY, "SunEye hourly shading..." ), 0, wxALL, 3 );
		import_tools->Add( new wxButton( m_scrollWin, ID_IMPORT_SUNEYE_OBSTRUCTIONS, "SunEye obstructions table..." ), 0, wxALL, 3 );
		import_tools->Add( new wxButton( m_scrollWin, ID_IMPORT_SOLPATH_MXH, "SolarPathfinder month by hour shading..." ), 0, wxALL, 3 );

		wxColour text_color( 0, 128, 192 );
		int wrap_width = 700;

		wxSizer *scroll = new wxBoxSizer( wxVERTICAL );
		scroll->Add( import_tools, 0, wxALL, 5 );
		scroll->Add( new wxStaticLine( m_scrollWin ), 0, wxALL|wxEXPAND );

		scroll->Add( m_enableTimestep, 0, wxALL|wxEXPAND, 5 );
		scroll->Add( m_textHourly = new wxStaticText( m_scrollWin, wxID_ANY, hourly_text ), 0, wxALL|wxEXPAND, 10 );
		scroll->Add( m_timestep, 0, wxALL, 5 );
		m_textHourly->Wrap( wrap_width );
		m_textHourly->SetForegroundColour( text_color );
		
		scroll->Add( new wxStaticLine( m_scrollWin ), 0, wxALL|wxEXPAND );
		scroll->Add( m_enableMxH, 0, wxALL|wxEXPAND, 5 );
		scroll->Add( m_textMxH = new wxStaticText( m_scrollWin, wxID_ANY, mxh_text ), 0, wxALL|wxEXPAND, 10 );
		scroll->Add( m_mxh, 0, wxALL, 5  );
		m_textMxH->Wrap( wrap_width );
		m_textMxH->SetForegroundColour( text_color );
		
		scroll->Add( new wxStaticLine( m_scrollWin ), 0, wxALL|wxEXPAND );
		scroll->Add( m_enableAzal, 0, wxALL|wxEXPAND, 5 );
		scroll->Add( m_textAzal = new wxStaticText( m_scrollWin, wxID_ANY, azal_text ), 0, wxALL|wxEXPAND, 10 );
		scroll->Add( m_azal, 0, wxALL, 5  );
		m_textAzal->Wrap( wrap_width );
		m_textAzal->SetForegroundColour( text_color );		

		scroll->Add( new wxStaticLine( m_scrollWin ), 0, wxALL|wxEXPAND );
		scroll->Add( m_enableDiffuse, 0, wxALL|wxEXPAND, 5 );
		scroll->Add( m_textDiffuse = new wxStaticText( m_scrollWin, wxID_ANY, diff_text ), 0, wxALL|wxEXPAND, 10 );
		scroll->Add( m_diffuseFrac, 0, wxALL, 5  );
		m_textDiffuse->Wrap( wrap_width );
		m_textDiffuse->SetForegroundColour( text_color );	

		m_scrollWin->SetSizer( scroll );

		wxSizer *buttons = new wxBoxSizer( wxHORIZONTAL );
		buttons->AddStretchSpacer(1);
		buttons->Add( new wxButton(this, wxID_OK, "OK" ), 0, wxALL|wxEXPAND, 3 );
		buttons->Add( new wxButton(this, wxID_CANCEL, "Cancel" ), 0, wxALL|wxEXPAND, 3  );
		buttons->Add( new wxButton(this, wxID_HELP, "Help" ), 0, wxALL|wxEXPAND, 3  );

		wxSizer *box = new wxBoxSizer(wxVERTICAL);
		box->Add( m_scrollWin, 1, wxALL|wxEXPAND );
		box->Add( buttons, 0, wxALL|wxEXPAND );
		SetSizer( box );

		UpdateVisibility();
	}

	void UpdateVisibility()
	{
		m_timestep->Show( m_enableTimestep->IsChecked() );
		m_textHourly->Show( m_enableTimestep->IsChecked() );
		
		m_mxh->Show( m_enableMxH->IsChecked() );
		m_textMxH->Show( m_enableMxH->IsChecked() );

		m_azal->Show( m_enableAzal->IsChecked() );
		m_textAzal->Show( m_enableAzal->IsChecked() );

		m_diffuseFrac->Show( m_enableDiffuse->IsChecked() );
		m_textDiffuse->Show( m_enableDiffuse->IsChecked() );

		m_scrollWin->FitInside();
		m_scrollWin->Refresh();
	}
	
	void ImportData( wxCommandEvent &e )
	{
		ShadingInputData dat;
		switch( e.GetId() )
		{
		case ID_IMPORT_PVSYST_NEAR_SHADING:
			if ( ImportPVsystNearShading( dat, this ) )
			{
				wxMessageBox( Load( dat, false ) );
				UpdateVisibility();
			}
			break;
		case ID_IMPORT_SUNEYE_HOURLY:
			if ( ImportSunEyeHourly( dat, this ) )
			{
				wxMessageBox( Load( dat, false ) );
				UpdateVisibility();
			}
			break;
		case ID_IMPORT_SUNEYE_OBSTRUCTIONS:
			if ( ImportSunEyeObstructions( dat, this ) )
			{
				wxMessageBox( Load( dat, false ) );
				UpdateVisibility();
			}
			break;
		case ID_IMPORT_SOLPATH_MXH:
			if ( ImportSolPathMonthByHour( dat, this ) )
			{
				wxMessageBox( Load( dat, false ) );
				UpdateVisibility();
			}
			break;
		}
	}
	
	void OnCommand( wxCommandEvent &e )
	{
		switch( e.GetId() )
		{
		case wxID_HELP:
			SamApp::ShowHelp("edit_shading_data");
			break;
		case ID_ENABLE_HOURLY:
		case ID_ENABLE_MXH:
		case ID_ENABLE_AZAL:
		case ID_ENABLE_DIFF:
			UpdateVisibility();
			break;
		}
	}
	
	
	void OnClose( wxCloseEvent & )
	{
		EndModal( wxID_CANCEL );
	}

	wxString Load( ShadingInputData &sh, bool all = true )
	{
		wxString stat;

		//if ( all || sh.en_hourly )
		//{
		//	m_enableHourly->SetValue( sh.en_hourly );
		//	stat += "Updated hourly beam shading losses.\n";
		//}
		if (all || sh.en_timestep)
		{
			m_enableTimestep->SetValue(sh.en_timestep);
			m_timestep->SetData(sh.timestep);
			stat += "Updated timestep beam shading losses.\n";
		}

		if ( all || sh.en_mxh )
		{
			m_enableMxH->SetValue( sh.en_mxh );
			m_mxh->SetData( sh.mxh );
			stat += "Updated month-by-hour beam shading loss table.\n";
		}

		if ( all || sh.en_azal )
		{
			m_enableAzal->SetValue( sh.en_azal );
			m_azal->SetData( sh.azal );
			stat += "Updated azimuth-by-altitude beam shading factor table.\n";
		}

		if ( all || sh.en_diff )
		{
			m_enableDiffuse->SetValue( sh.en_diff );
			m_diffuseFrac->SetValue( sh.diff );
			stat += "Updated constant sky diffuse factor.\n";
		}

		UpdateVisibility();

		/*
		// check that timestep number of rows = weather file rows
		// up to user or from Shade tool
		size_t nr = m_timestep->GetData().nrows();
		ssc_data_t pdata = ssc_data_create();
		//TODO var value for solar resource file
		wxString wf = "solar_resource_file";
		ssc_data_set_string(pdata, "file_name", wf);
		ssc_data_set_number(pdata, "header_only", 1);

		if (const char *err = ssc_module_exec_simple_nothread("wfreader", pdata))
		{
			wxLogStatus("error scanning '" + wf + "'");
			wxLogStatus("\t%s", err);
		}
		else
		{
			ssc_number_t num_wf_records;
			ssc_data_get_number(pdata,"nrecords", &num_wf_records);
			int wf_nrec = (int)num_wf_records;

			if (nr != (size_t)num_wf_records)
				m_timestep->SetNumRows(wf_nrec);
		}
		*/
		return stat;
	}

	void Save( ShadingInputData &sh )
	{
//		sh.en_hourly = m_enableTimestep->IsChecked();
//		m_hourly->Get( sh.hourly ); 

		sh.en_timestep = m_enableTimestep->IsChecked();
		m_timestep->GetData(sh.timestep);

		sh.en_mxh = m_enableMxH->IsChecked();
		sh.mxh.copy( m_mxh->GetData() );

		sh.en_azal = m_enableAzal->IsChecked();
		m_azal->GetData( sh.azal );

		sh.en_diff = m_enableDiffuse->IsChecked();
		sh.diff = m_diffuseFrac->Value();

		
	}

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE( ShadingDialog, wxDialog )
	EVT_CLOSE( ShadingDialog::OnClose )
	EVT_CHECKBOX( ID_ENABLE_HOURLY, ShadingDialog::OnCommand )
	EVT_CHECKBOX( ID_ENABLE_MXH, ShadingDialog::OnCommand )
	EVT_CHECKBOX( ID_ENABLE_AZAL, ShadingDialog::OnCommand )
	EVT_CHECKBOX( ID_ENABLE_DIFF, ShadingDialog::OnCommand )
	
	EVT_BUTTON( ID_IMPORT_PVSYST_NEAR_SHADING, ShadingDialog::ImportData )
	EVT_BUTTON( ID_IMPORT_SUNEYE_HOURLY, ShadingDialog::ImportData )
	EVT_BUTTON( ID_IMPORT_SUNEYE_OBSTRUCTIONS, ShadingDialog::ImportData )
	EVT_BUTTON( ID_IMPORT_SOLPATH_MXH, ShadingDialog::ImportData )

	EVT_BUTTON( wxID_HELP, ShadingDialog::OnCommand )

END_EVENT_TABLE()



BEGIN_EVENT_TABLE(ShadingButtonCtrl, wxButton)
EVT_BUTTON(wxID_ANY, ShadingButtonCtrl::OnPressed)
END_EVENT_TABLE()

ShadingButtonCtrl::ShadingButtonCtrl( wxWindow *parent, int id, 
	const wxPoint &pos, const wxSize &size)
	: wxButton( parent, id, "Edit shading...", pos, size )
{
}

void ShadingButtonCtrl::Write( VarValue *vv )
{
	m_shad.write( vv );
}

bool ShadingButtonCtrl::Read( VarValue *vv )
{
	return m_shad.read( vv );
}

void ShadingButtonCtrl::OnPressed(wxCommandEvent &evt)
{
	ShadingDialog dlg( this, m_descText );
	dlg.Load( m_shad );
	
	if (dlg.ShowModal()==wxID_OK)
	{
		dlg.Save( m_shad );
		evt.Skip(); // allow event to propagate indicating underlying value changed
	}
}


/***************************************************************
  Utility functions to import externally generated shading data
  **************************************************************/


bool ImportPVsystNearShading( ShadingInputData &dat, wxWindow *parent )
{
	//ask about version of PVsyst (5 versus 6) due to change in shading convention
	bool new_version = true;
	wxString msg = "Is this shading file from PVsyst version 6 or newer?";
	msg += "\n\nPVsyst changed their shading convention starting in Version 6 and later such that 0 now equals no shade and 1 equals full shade. ";
	msg += "To import a file from PVsyst versions 6 or newer, click Yes below. However, you may still import a file from versions 5 and older by selecting No below.";
	int ret = wxMessageBox(msg, "Important Notice", wxICON_EXCLAMATION | wxYES_NO, parent);
	if (ret == wxNO)
		new_version = false;

	//read in the file
	wxString buf;
	double diffuse = 0.0;
	int i;
	int j;

	bool readdata = false;
	bool readok = true;
	bool headingok = true;
	bool colok = true;
	int linesok = 0;

		
	matrix_t<float> azaltvals;
	azaltvals.resize_fill(11,20, 0.0f);
	azaltvals.at(0,0) = 0.0f;

	for (i=1;i<20;i++) azaltvals.at(0,i) = 20*(i-1);  // removed -180 degree offset to account for new azimuth convention (180=south) 4/2/2012, apd
	for (i=1;i<10;i++) azaltvals.at(i,0) = 10*i;
	azaltvals.at(10,0) = 2.0f;

	wxFileDialog fdlg(parent, "Import PVsyst Near Shading File");
	if (fdlg.ShowModal() != wxID_OK) return false;
	wxString file = fdlg.GetPath();

	wxTextFile tf;
	if ( !tf.Open( file ) )
	{
		wxMessageBox("Could not open file for read:\n\n" + file);
		return false;
	}

	j = 0;
	buf = tf.GetFirstLine();
	while( !tf.Eof() )
	{
		wxArrayString lnp = wxStringTokenize( buf, ";:,\t" );
		if (readdata == false && j == 0)
		{
			if (lnp.Count() > 0)
			{
				if (lnp.Item(0) == "Height") readdata = true;
			}
		}
		else if (j < 10)
		{
			j++;
			if (lnp.Count() != 20)
			{
				colok = false;
				readok = false;
				break;
			}
			else
			{
				for (i = 0; i<20; i++) // read in Altitude in column zero
				{
					if (i == 0) azaltvals.at(j, i) = (float)wxAtof(lnp[i]);		//do not change azimuth values
					else
					{
						if (lnp.Item(i) == "Behind")
							azaltvals.at(j, i) = 100;	//"Behind" means it is fully behind another obstruction
						else if (new_version) //PVsyst versions 6 and newer: 0 means no shade, 1 means full shade
							azaltvals.at(j, i) = (float)wxAtof(lnp[i]) * 100; //convert to percentage
						else //PVsyst versions 5 and older: 1 means no shade, 0 means full shade
							azaltvals.at(j, i) = (1 - (float)wxAtof(lnp[i])) * 100;	//convert from factor to loss
					}
				}
			}
		}
		else if (j == 10)
		{
			if (lnp.Count()== 3)
			{
				if (new_version) //PVsyst versions 6 and newer: 0 means no shade, 1 means full shade
					diffuse = (float)wxAtof(lnp[1]) * 100; //convert to percentage
				else //PVsyst versions 5 and older: 1 means no shade, 0 means full shade
					diffuse = (1 - (float)wxAtof(lnp[1])) * 100; //convert from factor to loss
			}
			else
			{
				readok = false;
				colok = false;
				break;
			}
			j++;
		}
		else j++;

		buf = tf.GetNextLine();
	}
	
	if (j < 11)
	{
		readok = false;
		linesok = -1;
	}
	else if (j > 11)
	{
		readok = false;
		linesok = 1;
	}
	if (readdata != true)
	{
		readok = false;
		headingok = false;
	}
	
	if (readok)
	{
		// re-sort from small altitude to large, if necessary 
		if (azaltvals.at(10, 0) < azaltvals.at(1, 0))
		{
			azaltvals.resize_preserve(12, 20, 1.0);

			for (j = 1; j < 12 / 2; j++)
			{
				for (i = 0; i < 20; i++) azaltvals.at(11, i) = azaltvals.at(j, i);
				for (i = 0; i < 20; i++) azaltvals.at(j, i) = azaltvals.at(11-j, i);
				for (i = 0; i < 20; i++) azaltvals.at(11-j, i) = azaltvals.at(11, i);
			}

			azaltvals.resize_preserve(11, 20, 1.0);
		}

		dat.clear();
		dat.en_azal = true;
		dat.azal.copy( azaltvals );

		dat.en_diff = true;
		dat.diff = diffuse;

		return true;
	}
	else
	{
		wxString m = "Invalid file format.\n\n";
		if (!headingok)	m.Append("Invalid heading format.\n");
		if (!colok) m.Append("Invalid number of columns.\n");
		if (linesok == -1) m.Append("File contains fewer lines than expected.\n");
		if (linesok == 1) m.Append("File contains more lines than expected.\n");
		wxMessageBox(m);

		return false;
	}
}
	
bool ImportSunEyeHourly( ShadingInputData &dat, wxWindow *parent )
{
	wxFileDialog fdlg(parent, "Import SunEye Shading File");
	if (fdlg.ShowModal() != wxID_OK) return false;
	
	wxTextFile tf;
	if ( !tf.Open( fdlg.GetPath() ) )
	{
		wxMessageBox("Could not open file for read:\n\n" + fdlg.GetPath());
		return false;
	}

	wxString buf;
	int i;
	bool readdata = false;
	bool readok = true;
	bool headingok = true;
	bool colok = true;
	int linesok = 0;
	int day = 0;
	int start_timestep=0;
	int start_hour=0;
	int end_timestep=0;
	int end_hour=0;
	int hour_duration=0; // how many hours (including incomplete hours) in the Suneye file
	double beam[8760];
	for (i=0;i<8760;i++) beam[i]=0.0;

	buf = tf.GetFirstLine();
	while( !tf.Eof() )
	{
		wxArrayString lnp = wxStringTokenize(buf, ",", wxTOKEN_RET_EMPTY_ALL);
		if (readdata == false)
		{
			if (lnp.Count() > 0)
			{
				if (lnp.Item(0) == "begin data") 
				{
					readdata = true;
					int iend = lnp.Count()-1;
					int icolon = 0;
					icolon = lnp.Item(1).find(":");
					if (icolon>0) 
					{
						start_timestep = wxAtoi(lnp.Item(1).substr(icolon+1,2));
						start_hour = wxAtoi(lnp.Item(1).substr(0,icolon));
					}
					icolon = lnp.Item(iend).find(":");
					if (icolon>0) 
					{
						end_timestep = wxAtoi(lnp.Item(iend).substr(icolon+1,2));
						end_hour = wxAtoi(lnp.Item(iend).substr(0,icolon));
					}
					// check for valid duration
					if ((start_hour==0) || (end_hour==0))
					{
						readdata=false;
						break;
					}
					hour_duration = end_hour - start_hour + 1;
				}
			}
		}
		else
		{
			// shj update 5/25/11 - read in begin data and to end - no fixed count
			// assume that 15 timestep intervals and use start and end time and adjust to hour
			// JMF update 10/17/2014- average all values for an hour instead of taking the midpoint of the hour
			int index = 1; //keep track of where you are in the row- starts at 1 because of the date column.
			for (i=0;i<hour_duration;i++)
			{
				//compute which hour to enter the shading factor into
				int x = day*24+start_hour+i;
				if (x >= 8760)
				{
					readok = false;
					break;
				}

				//how many 15-min entries are in this hour?
				int count = 0;
				if (i == 0) //first hour
					count = (60 - start_timestep) / 15;
				else if (i == hour_duration - 1) //last hour
					count = end_timestep / 15 + 1;
				else //whole hours in between
					count = 4;

				//loop through the correct number of 15-timestep entries and to calculate an average shading value
				double total = 0;
				for (int j = 0; j < count; j++)
				{
					if (lnp.Item(index).IsEmpty()) total += 0;
					else total += wxAtof(lnp.Item(index));
					index++; //don't forget to increment the index so that you read the next cell
				}
				
				//compute average and assign it to the appropriate hour
				beam[x] = (1 - (total / count)) * 100; //don't forget to convert it to a loss factor
			}
			day++;
		}

		buf = tf.GetNextLine();
	}

	if (day !=365)
	{
		readok = false;
		if (day - 365 < 0 && day != 0) linesok = -1;
		if (day - 365 > 0 && day != 0) linesok = 1;
	}

	if (readdata == false)
	{
		readok = false;
		headingok = false;
	}
	
	if (readok)
	{
		dat.clear();
		//dat.en_hourly = true;
		//dat.hourly.clear();
		//dat.hourly.reserve( 8760 );
		//for( size_t i=0;i<8760;i++ ) 
		//	dat.hourly.push_back( beam[i] );
		dat.en_timestep = true;
		dat.timestep.clear();
		dat.timestep.resize_fill(8760,1,0);
		for (size_t i = 0; i<8760; i++)
			dat.timestep.at(i,0) = beam[i];
		return true;
	}
	else
	{
		wxString m = "Invalid file format.\n\n";
		if (!headingok)	m.Append("Invalid heading format.\n");
		if (!colok) m.Append("Invalid number of columns.\n");
		if (linesok == -1) m.Append("File contains fewer lines than expected.\n");
		if (linesok == 1) m.Append("File contains more lines than expected.\n");
		wxMessageBox(m);
		return false;
	}

}
	
bool ImportSunEyeObstructions( ShadingInputData &dat, wxWindow *parent )
{


	wxFileDialog fdlg(parent, "Import SunEye Obstruction Elevations File");
	if (fdlg.ShowModal() != wxID_OK) return false;
	wxString file = fdlg.GetPath();
	wxTextFile tf;
	if ( !tf.Open( file ) )
	{
		wxMessageBox("Could not open file for read:\n\n" + file);
		return false;
	}

	wxString buf;
	int j = -2;
	bool readdata = false;
	bool readok = true;
	bool headingok = true;
	bool colok = true;
	int linesok = 0;
	double azi[361];
	double alt[361];
	int imageCount;
	float obstructionStep;
	double tmpVal;
		
	matrix_t<float> azaltvals, obstructions;
	azaltvals.resize_fill(91,362, 0.0);
	azaltvals.at(0,0) = 0.;

	buf = tf.GetFirstLine();

	while( !tf.Eof() )
	{
		wxArrayString lnp = wxStringTokenize( buf, "," );
		if (readdata == false)
		{
			if (lnp.Count() > 0 && j == -2)
			{
				if (lnp.Item(0) == "begin data")
				{
					readdata = true;
					j++;
				}
			}
		}
		else if (readdata == true && j == -1) j++;
		else
		{
			// Sev 150624: Check for number of images
			if( j == 0)	{
				imageCount = lnp.Count() - 4;
				obstructions.resize_fill( 362, imageCount, 0.0);
				obstructionStep = 100.0 / imageCount;
			}

			if (j <= 360)
			{
// Modify to read in ObstructionElevation.csv (average in column 3 and then max and then value for each skyline
// Works with individual skyline obstrucitons (e.g. Sky01ObstrucitonElevations.csv) or with average.
//				if (lnp.Count() != 3)
				if (lnp.Count() < imageCount + 3)
				{
					colok = false;
					readok = false;
					break;
				} 
				else
				{
					azi[j] = wxAtof(lnp[0]); //first column contains compass heading azimuth values (0=north, 90=east)
					alt[j] = wxAtof(lnp[2]);
					for (int ii=0; ii<imageCount; ii++)
						obstructions.at(j,ii) = wxAtof(lnp[ii+4]); 
				}
				j++;
			}
			else j++;
		}
		buf = tf.GetNextLine();
	}


	if (j < 361)
	{
		readok = false;
		linesok = -1;
	}
	else if (j > 361)
	{
		readok = false;
		linesok = 1;
	}
	if (readdata != true)
	{
		readok = false;
		headingok = false;
	}

	if (readok)
	{
		//copy azimuth/compass values into the first row
		for (int i=1;i<362;i++)
			azaltvals.at(0,i) = azi[i-1];

		//elevation always goes from 1-90 degrees
		for (int i=1;i<91;i++)
			azaltvals.at(i,0) = i;

		//loop through all azimuth values
		for (int j=0;j<362;j++)
		{
			for(int k=0; k<imageCount;k++){  // Sev 150624: loop over images
				
				if (obstructions.at(j,k)<0 || obstructions.at(j,k)>90) //changed from && to || 6/18/15 jmf
				{
					wxMessageBox("Error: Elevations Must be less than 90 degrees and greater than 0 degrees");
					return false;
				}

				//changed jmf 6/18/15- values UP TO the altitude value should be fully shaded.
				for (int i = 1; i <= obstructions.at(j,k); i++)
					azaltvals.at(i, j + 1) += obstructionStep;		// Sev 150624: obstruction amount now increase incrementally instead of going straight from 0 to 100
			}
		}

		dat.clear();

		dat.en_azal = true;
		dat.azal = azaltvals;

		return true;
	}
	else
	{
		wxString m = "Invalid file format.\n\n";
		if (!headingok)	m.Append("Invalid heading format.\n");
		if (!colok) m.Append("Invalid number of columns.\n");
		if (linesok == -1) m.Append("File contains fewer lines than expected.\n");
		if (linesok == 1) m.Append("File contains more lines than expected.\n");
		wxMessageBox(m);
		
		return false;
	}
}

bool ImportSolPathMonthByHour( ShadingInputData &dat, wxWindow *parent )
{
	wxFileDialog fdlg(parent, "Import Solar Pathfinder Month By Hour Shading File");
	if (fdlg.ShowModal() != wxID_OK) return false;
	wxString file = fdlg.GetPath();

	wxTextFile tf;
	if ( !tf.Open( file ) )
	{
		wxMessageBox("Could not open file for read:\n\n" + file);
		return false;
	}

	// read in month by hour grid from first image in shading file - Oliver Hellwig - Solar Pathfinder programmer - every half hour - read 2 value and average for now
	// 12:00 AM	12:30 AM	1:00 AM	1:30 AM	2:00 AM	2:30 AM	3:00 AM	3:30 AM	4:00 AM	4:30 AM	5:00 AM	5:30 AM	6:00 AM	6:30 AM	7:00 AM	7:30 AM	8:00 AM	8:30 AM	9:00 AM	9:30 AM	10:00 AM	10:30 AM	11:00 AM	11:30 AM	12:00 PM	12:30 PM	1:00 PM	1:30 PM	2:00 PM	2:30 PM	3:00 PM	3:30 PM	4:00 PM	4:30 PM	5:00 PM	5:30 PM	6:00 PM	6:30 PM	7:00 PM	7:30 PM	8:00 PM	8:30 PM	9:00 PM	9:30 PM	10:00 PM	10:30 PM	11:00 PM	11:30 PM
	// 12,24,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
	// double array 12rowsx24columns Solar pathfinder percentages to fractions

	wxString buf;
	int i, imageCount = 0;
	bool readdata = false;
	bool readok = true;
	bool headingok = true;
	int month=0; // 
	double beam[290];
	for (i=0;i<290;i++) beam[i]=0.0;
	beam[0]=12;
	beam[1]=24;

	buf = tf.GetFirstLine();

// data at half hour is recorded for hour in 8760 shading file - e.g. Jan-1 5:30 data recoded at hour 5
	while( !tf.Eof() )
	{
		wxArrayString lnp = wxStringTokenize(buf, ",", wxTOKEN_RET_EMPTY_ALL);
		if (readdata == false)
		{
			if (lnp.Count() > 0)
			{
				if ( ((std::string)(lnp.Item(0))).find("Image Layout Number ")==0 )
				{
					imageCount++;
					month = 0;
					buf = tf.GetNextLine();
					readdata = true;
				}
			}
		}
		else
		{
			if (month==11)
			{
				readdata = false;
			}
			for (i=0;i<24;i++)
			{
				int ndex=i+month*24+2; // skip 12x24 array size
				if (ndex > 289) 
				{
					readok=false;
					break;
				}
				// average hour and half hour values starting at midnight (skip row label)
				if (imageCount==1){
					beam[ndex] = 100- (wxAtof(lnp.Item(2*i+1))+wxAtof(lnp.Item(2*i+1+1)))/2.0;	//convert from a factor to a loss
				} else {
					beam[ndex] += 100- (wxAtof(lnp.Item(2*i+1))+wxAtof(lnp.Item(2*i+1+1)))/2.0;	//convert from a factor to a loss
				}
			}
			month++;
		}
		buf = tf.GetNextLine();
		if (tf.Eof() ) readdata = true;
	}

	if (readdata == false || imageCount == 0)
	{
		readok = false;
		headingok = false;
	}

	if (readok)
	{
		dat.clear();
		dat.en_mxh = true;
		dat.mxh.resize_fill(12,24, 0.0);
		for (int r=0;r<12;r++)
			for (int c=0;c<24;c++)
				dat.mxh.at(r,c) = beam[ 24*r+c+2 ] / imageCount;
		return true;
	}
	else
	{
		wxString m = "Invalid file format.\n\n";
		wxMessageBox(m);
		return false;
	}
}




DEFINE_EVENT_TYPE(wxEVT_wxShadingFactorsCtrl_CHANGE)

enum { ISFC_CHOICECOL = wxID_HIGHEST + 857, ISFC_CHOICEMINUTE, ISFC_GRID };

BEGIN_EVENT_TABLE(wxShadingFactorsCtrl, wxPanel)
EVT_GRID_CMD_CELL_CHANGE(ISFC_GRID, wxShadingFactorsCtrl::OnCellChange)
EVT_CHOICE(ISFC_CHOICECOL, wxShadingFactorsCtrl::OnChoiceCol)
EVT_CHOICE(ISFC_CHOICEMINUTE, wxShadingFactorsCtrl::OnChoiceMinute)
END_EVENT_TABLE()

wxShadingFactorsCtrl::wxShadingFactorsCtrl(wxWindow *parent, int id,
const wxPoint &pos,
const wxSize &sz,
bool sidebuttons)
: wxPanel(parent, id, pos, sz)
{
	m_default_val = 0;
	m_num_timesteps = 60;
	m_col_header_use_format = false;
	m_col_ary_str.Clear();
	m_col_format_str = wxEmptyString;
	m_col_arystrvals.Clear();
	m_timestep_arystrvals.Clear();

	m_grid = new wxExtGridCtrl(this, ISFC_GRID);
	m_grid->CreateGrid(8760, 8);
	m_grid->EnableCopyPaste(true);
	m_grid->EnablePasteEvent(true);
	m_grid->DisableDragCell();
	m_grid->DisableDragRowSize();
	m_grid->DisableDragColMove();
	m_grid->DisableDragGridSize();
	m_grid->SetRowLabelAlignment(wxALIGN_LEFT, wxALIGN_CENTER);

	m_caption_col = new wxStaticText(this, wxID_ANY, "");
	m_caption_col->SetFont(*wxNORMAL_FONT);

	m_col_arystrvals.push_back("1");
	m_col_arystrvals.push_back("2");
	m_col_arystrvals.push_back("3");
	m_col_arystrvals.push_back("4");
	m_col_arystrvals.push_back("5");
	m_col_arystrvals.push_back("6");
	m_col_arystrvals.push_back("7");
	m_col_arystrvals.push_back("8");
	m_choice_col = new wxChoice(this, ISFC_CHOICECOL, wxDefaultPosition, wxDefaultSize, m_col_arystrvals);
	m_choice_col->SetBackgroundColour(*wxWHITE);

	m_caption_timestep = new wxStaticText(this, wxID_ANY, "");
	m_caption_timestep->SetFont(*wxNORMAL_FONT);

	m_timestep_arystrvals.push_back("1");
	m_timestep_arystrvals.push_back("2");
	m_timestep_arystrvals.push_back("3");
	m_timestep_arystrvals.push_back("4");
	m_timestep_arystrvals.push_back("5");
	m_timestep_arystrvals.push_back("6");
	m_timestep_arystrvals.push_back("10");
	m_timestep_arystrvals.push_back("12");
	m_timestep_arystrvals.push_back("15");
	m_timestep_arystrvals.push_back("20");
	m_timestep_arystrvals.push_back("30");
	m_timestep_arystrvals.push_back("60");
	m_choice_timestep = new wxChoice(this, ISFC_CHOICEMINUTE, wxDefaultPosition, wxDefaultSize, m_timestep_arystrvals);
	m_choice_timestep->SetBackgroundColour(*wxWHITE);


	if (sidebuttons)
	{
		// for side buttons layout
		wxBoxSizer *v_tb_sizer = new wxBoxSizer(wxVERTICAL);
		v_tb_sizer->Add(m_caption_col, 0, wxALL | wxEXPAND, 3);
		v_tb_sizer->Add(m_choice_col, 0, wxALL | wxEXPAND, 3);
		v_tb_sizer->AddSpacer(5);
		v_tb_sizer->Add(m_caption_timestep, 0, wxALL | wxEXPAND, 3);
		v_tb_sizer->Add(m_choice_timestep, 0, wxALL | wxEXPAND, 3);
		v_tb_sizer->AddStretchSpacer();

		wxBoxSizer *h_sizer = new wxBoxSizer(wxHORIZONTAL);
		h_sizer->Add(v_tb_sizer, 0, wxALL | wxEXPAND, 1);
		h_sizer->Add(m_grid, 1, wxALL | wxEXPAND, 1);

		SetSizer(h_sizer);
	}
	else
	{
		// for top buttons layout (default)
		wxBoxSizer *h_tb_sizer = new wxBoxSizer(wxHORIZONTAL);
		h_tb_sizer->Add(m_caption_col, 0, wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 3);
		h_tb_sizer->Add(m_choice_col, 0, wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 3);
		h_tb_sizer->AddSpacer(5);
		h_tb_sizer->Add(m_caption_timestep, 0, wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 3);
		h_tb_sizer->Add(m_choice_timestep, 0, wxALL | wxEXPAND | wxALIGN_CENTER_VERTICAL, 3);
		h_tb_sizer->AddStretchSpacer();

		wxBoxSizer *v_sizer = new wxBoxSizer(wxVERTICAL);
		v_sizer->Add(h_tb_sizer, 0, wxALL | wxEXPAND, 1);
		v_sizer->Add(m_grid, 1, wxALL | wxEXPAND, 1);

		SetSizer(v_sizer, false);
	}

	MatrixToGrid();
}


void wxShadingFactorsCtrl::UpdateNumberColumns(size_t &new_cols)
{
	// resize and preserve existing data and fill new data with default.
	m_data.resize_preserve(m_num_rows, new_cols, m_default_val);
	MatrixToGrid();
}

void wxShadingFactorsCtrl::UpdateNumberRows(size_t &new_rows)
{
	// resize and preserve existing data and fill new data with default.
	m_data.resize_preserve(new_rows, m_num_cols, m_default_val);
	MatrixToGrid();
}

void wxShadingFactorsCtrl::UpdateNumberMinutes(size_t &new_timesteps)
{
	// resize and preserve existing data and fill new data with default.
	// multiple of 8760 timesteps to number of timesteps
	if ((new_timesteps > 0) && (new_timesteps <= 60))
	{
		size_t new_rows = 60 / new_timesteps * 8760;
		m_data.resize_preserve(new_rows, m_num_cols, m_default_val);
		MatrixToGrid();
	}
}


void wxShadingFactorsCtrl::UpdateColumnHeaders()
{
	if (m_col_header_use_format)
	{
		for (size_t i = 0; i < m_grid->GetNumberCols(); i++)
		{
			if (m_col_format_str.Find("%d") != wxNOT_FOUND)
				m_grid->SetColLabelValue(i, wxString::Format(m_col_format_str, i));
			else
				m_grid->SetColLabelValue(i, m_col_format_str);
		}
	}
	else
	{
		for (size_t i = 0; i < m_grid->GetNumberCols() && i < m_col_ary_str.Count(); i++)
			m_grid->SetColLabelValue(i, m_col_ary_str[i]);
	}
}



void wxShadingFactorsCtrl::SetColLabelFormatString(const wxString &col_format_str)
{
	if (col_format_str != m_col_format_str)
		m_col_format_str = col_format_str;
	m_col_header_use_format = false;
	UpdateColumnHeaders();
}



void wxShadingFactorsCtrl::OnChoiceCol(wxCommandEvent  &evt)
{
	if ((m_choice_col->GetSelection() != wxNOT_FOUND) && (wxAtoi(m_choice_col->GetString(m_choice_col->GetSelection())) != m_num_cols))
	{ 
		size_t new_cols = wxAtoi(m_choice_col->GetString(m_choice_col->GetSelection()));
		UpdateNumberColumns(new_cols);
	}
}

void wxShadingFactorsCtrl::OnChoiceMinute(wxCommandEvent  &evt)
{
	if ((m_choice_timestep->GetSelection() != wxNOT_FOUND) && (wxAtoi(m_choice_timestep->GetString(m_choice_timestep->GetSelection())) != m_num_timesteps))
	{
		m_num_timesteps = wxAtoi(m_choice_timestep->GetString(m_choice_timestep->GetSelection()));
		UpdateNumberMinutes(m_num_timesteps);
	}
}

void wxShadingFactorsCtrl::SetData(const matrix_t<float> &mat)
{
	m_data = mat;
	MatrixToGrid();
}

void wxShadingFactorsCtrl::GetData(matrix_t<float> &mat)
{
	mat = m_data;
}



void wxShadingFactorsCtrl::OnCellChange(wxGridEvent &evt)
{
	int irow = evt.GetRow();
	int icol = evt.GetCol();

	if (irow == -1 && icol == -1) // paste event generated from base class
	{
		for (int ir = 0; ir < m_grid->GetNumberRows(); ir++)
			for (int ic = 0; ic < m_grid->GetNumberCols(); ic++)
			{
				float val = (float)wxAtof(m_grid->GetCellValue(ir, ic).c_str());
				m_data.at(ir, ic) = val;
				m_grid->SetCellValue(ir, ic, wxString::Format("%g", val));
			}
	}
	else
	{
		float val = (float)wxAtof(m_grid->GetCellValue(irow, icol).c_str());

		if (irow < m_data.nrows() && icol < m_data.ncols()
			&& irow >= 0 && icol >= 0)
			m_data.at(irow, icol) = val;

		m_grid->SetCellValue(irow, icol, wxString::Format("%g", val));
	}
	wxCommandEvent dmcevt(wxEVT_wxShadingFactorsCtrl_CHANGE, this->GetId());
	dmcevt.SetEventObject(this);
	GetEventHandler()->ProcessEvent(dmcevt);
}



void wxShadingFactorsCtrl::MatrixToGrid()
{
	int r, nr = m_data.nrows();
	int c, nc = m_data.ncols();


//	int ndx = m_choice_col->FindString(wxString::Format("%d",nc));
//	if (ndx != wxNOT_FOUND)
//		m_choice_col->SetSelection(ndx);

	m_num_cols = nc;
	m_num_rows = nr;

	m_grid->ResizeGrid(nr, nc);
	for (r = 0; r<nr; r++)
		for (c = 0; c<nc; c++)
			m_grid->SetCellValue(r, c, wxString::Format("%g", m_data.at(r, c)));

	UpdateColumnHeaders();
	UpdateRowLabels();
}


void wxShadingFactorsCtrl::SetColCaption(const wxString &cap)
{
	m_caption_col->SetLabel(cap);
	this->Layout();
}

wxString wxShadingFactorsCtrl::GetColCaption()
{
	return m_caption_col->GetLabel();
}


void wxShadingFactorsCtrl::SetMinuteCaption(const wxString &cap)
{
	m_caption_timestep->SetLabel(cap);
	this->Layout();
}

wxString wxShadingFactorsCtrl::GetMinuteCaption()
{
	return m_caption_timestep->GetLabel();
}


void wxShadingFactorsCtrl::SetNumCols(size_t &cols)
{
	UpdateNumberColumns(cols);
}

void wxShadingFactorsCtrl::SetNumRows(size_t &rows)
{
	UpdateNumberRows(rows);
}

void wxShadingFactorsCtrl::SetColLabels(const wxArrayString &colLabels)
{
	if (colLabels != m_col_ary_str)
		m_col_ary_str = colLabels;
	m_col_header_use_format = false;
	UpdateColumnHeaders();
}

wxArrayString wxShadingFactorsCtrl::GetColLabels()
{
	wxArrayString ret;
	for (size_t i = 0; i < m_grid->GetNumberCols(); i++)
		ret.push_back(m_grid->GetColLabelValue(i));
	return ret;
}

void  wxShadingFactorsCtrl::UpdateRowLabels()
{ // can do with GridTableBase like AFFloatArrayTable in widgets.cpp
	size_t num_rows = m_grid->GetNumberRows();
	if (num_rows > 8760) // otherwise use default row numbering
	{
		int nmult = num_rows / 8760;
		if (nmult != 0)
		{
			for (size_t row = 0; row < num_rows; row++)
			{
				double step = 1.0 / ((double)nmult);
				double tm = step*(row + 1);
				double frac = tm - ((double)(int)tm);
				if (frac == 0.0)
					m_grid->SetRowLabelValue(row, wxString::Format("%lg", tm));
				else
					m_grid->SetRowLabelValue(row, wxString::Format("   .%lg", frac * 60));
			}
		}
	}
	else
	{
		for (size_t row = 0; row < num_rows; row++)
			m_grid->SetRowLabelValue(row, wxString::Format("%d", row+1));
	}
}
