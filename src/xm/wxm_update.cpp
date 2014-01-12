///////////////////////////////////////////////////////////////////////////////
// vim:         sw=4 ts=4
// Name:        xm/wxm_update.cpp
// Description: Checking for update of wxMEdit
// Author:      wxmedit@gmail.com
// Licence:     GPL
///////////////////////////////////////////////////////////////////////////////

#include "wxm_update.h"
#include "xm_remote.h"
#include "wxm_def.h"
#include "../wxmedit_frame.h"
#include "../wxm_utils.h"

#include <wx/msgdlg.h>
#include <wx/intl.h>
#include <wx/thread.h>
#include <wx/fileconf.h>
#include <boost/xpressive/xpressive_dynamic.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/foreach.hpp>
#include <vector>
#include <algorithm>
#include <limits>

namespace xp =  boost::xpressive;
namespace algo = boost::algorithm;
namespace wxm
{

static std::string GetVersionFromRemoteChangeLog();
static std::vector<unsigned int> ParseVersion(const std::string& ver);
static bool IsFirstNewer(const std::vector<unsigned int>& v1, const std::vector<unsigned int>& v2);

inline bool IsFirstNewer(const std::string& v1, const std::string& v2)
{
	return IsFirstNewer(ParseVersion(v1), ParseVersion(v2));
}

std::string CheckUpdates()
{
	std::string remote_ver = GetVersionFromRemoteChangeLog();
	if (IsFirstNewer(remote_ver, WXMEDIT_VERSION))
		return remote_ver;

	return std::string();
}

void ConfirmUpdate(const std::string& newver, bool notify_newest)
{
	if (!newver.empty())
	{
		wxString title(_("wxMEdit - New version available"));
		wxString msg( wxString::Format( _("wxMEdit %s is available. \nClick OK to open the download page."), 
		                                wxString(newver.c_str(), wxConvUTF8).c_str() ) );
		if (wxOK == wxMessageBox(msg, title, wxICON_INFORMATION|wxOK|wxCANCEL, g_MainFrame))
			OpenURL(wxT("http://code.google.com/p/wxmedit/wiki/download?tm=2"));

		return;
	}

	if (notify_newest)
		wxMessageBox(_("wxMEdit is already the newest version."), wxT("wxMEdit"), wxICON_INFORMATION|wxOK, g_MainFrame);
}

std::string GetVersionFromRemoteChangeLog()
{
	std::string str(xm::GetRemoteText("https://raw.github.com/hltj/wxMEdit/master/ChangeLog"));

	size_t poslf1 = str.find_first_of('\n', 0);
	size_t poslf2 = str.find_first_of('\n', poslf1+1);
	size_t poslf3 = str.find_first_of('\n', poslf2+1);

	std::string ver_line = str.substr(poslf2+1, poslf3-poslf2-1);

	xp::sregex ver_regex = xp::sregex::compile("[0-9]+\\.[0-9]+\\.[0-9]+([\\.-][0-9]+)?");
	xp::smatch what;
	if (xp::regex_search(ver_line, what, ver_regex))
	{
		return what[0].str();
	}

	return std::string();
}

std::vector<unsigned int> ParseVersion(const std::string& ver)
{
	std::vector<std::string> svec;
	algo::split(svec, ver, algo::is_any_of(".-"));


	std::vector<unsigned int> uvec = boost::assign::list_of(0u)(0u)(0u)(0u);

	size_t len = std::min(uvec.size(), svec.size());
	try
	{
		for(size_t i=0; i<len; ++i)
			uvec[i] = boost::lexical_cast<unsigned int>(svec[i]);
	}
	catch (boost::bad_lexical_cast&) {}

	return uvec;
}

bool IsFirstNewer(const std::vector<unsigned int>& v1, const std::vector<unsigned int>& v2)
{
	for(size_t i=0; i<4; ++i)
	{
		if (v1[i] > v2[i])
			return true;
	}

	return false;
}

UpdatePeriods& UpdatePeriods::Instance()
{
	static UpdatePeriods s_inst;
	if (!s_inst.m_initialized)
		s_inst.Initialize();
	return s_inst;
}

void UpdatePeriods::AddData(int idx, time_t peroid, const wxString& cfg, const wxString& l10ntitle)
{
	m_idx_peroid_map[idx] = peroid;
	m_idx_l10ntitle_map[idx] = l10ntitle;
	m_l10ntitle_cfg_map[l10ntitle] = cfg;
	m_cfg_idx_map[cfg] = idx;
}

void UpdatePeriods::Initialize()
{
	if (m_initialized)
		return;

	m_initialized = true;
	time_t timemax = std::numeric_limits<time_t>::max();

	AddData(UP_DAILY,   time_t(3600*24),    wxT("daily"),   _("every day"));
	AddData(UP_WEEKLY,  time_t(3600*24*7),  wxT("weekly"),  _("every week"));
	AddData(UP_MONTHLY, time_t(3600*24*30), wxT("monthly"), _("every month"));
	AddData(UP_NEVER,   timemax,            wxT("never"),   _("never"));
}

std::vector<wxString> UpdatePeriods::GetTitles() const
{
	std::vector<wxString> l10ntitles;
	BOOST_FOREACH(const IdxL10nTitleMap::value_type& v, m_idx_l10ntitle_map)
		l10ntitles.push_back(v.second);
	return l10ntitles;
}

wxString UpdatePeriods::GetDefaultTitle() const
{
	return IndexToTitle(default_idx);
}

wxString UpdatePeriods::TitleToConfig(const wxString& title) const
{
	L10nTitleCfgMap::const_iterator it = m_l10ntitle_cfg_map.find(title);
	if (it != m_l10ntitle_cfg_map.end())
		return it->second;

	return wxString();
}

wxString UpdatePeriods::ConfigToTitle(const wxString& cfg) const
{
	return IndexToTitle(ConfigToIndex(cfg));
}

int UpdatePeriods::ConfigToIndex(const wxString& cfg) const
{
	CfgIdxMap::const_iterator it = m_cfg_idx_map.find(cfg);
	if (it != m_cfg_idx_map.end())
		return it->second;

	return default_idx;
}

wxString UpdatePeriods::IndexToTitle(int idx) const
{
	IdxL10nTitleMap::const_iterator it = m_idx_l10ntitle_map.find(idx);
	if (it != m_idx_l10ntitle_map.end())
		return it->second;

	return wxString();
}

time_t UpdatePeriods::ConfigToPeroid(const wxString& cfg) const
{
	return IndexToPeriod(ConfigToIndex(cfg));
}

time_t UpdatePeriods::IndexToPeriod(int idx) const
{
	IdxPeriodMap::const_iterator it = m_idx_peroid_map.find(idx);
	if (it != m_idx_peroid_map.end())
		return it->second;

	return default_peroid;
}

struct UpdatesCheckingThread : public wxThread
{
	virtual ExitCode Entry()
	{
		bool notify_newest = false;

#ifdef _DEBUG
		notify_newest = true;
#endif

		wxm::ConfirmUpdate(wxm::CheckUpdates(), notify_newest);
		return 0;
	}
};

void AutoCheckUpdates(wxFileConfig* cfg)
{
	long lasttime = 0;
	wxString period;
    cfg->Read(wxT("/wxMEdit/LastTimeAutoCheckUpdates"), &lasttime, 0);
    cfg->Read(wxT("/wxMEdit/UpdatesCheckingPeriod"), &period);

	if (time(NULL)-lasttime < UpdatePeriods::Instance().ConfigToPeroid(period))
		return;

	cfg->Write(wxT("/wxMEdit/LastTimeAutoCheckUpdates"), long(time(NULL)));

	UpdatesCheckingThread* t = new UpdatesCheckingThread();
	if (t->Create()==wxTHREAD_NO_ERROR && t->Run()==wxTHREAD_NO_ERROR)
		return;

#ifdef _DEBUG
	wxMessageBox(wxT("Automatically Update Thread Error."), wxT("wxMEdit - Error"), wxICON_ERROR|wxOK, g_MainFrame);
#endif
}

} //namespace wxm