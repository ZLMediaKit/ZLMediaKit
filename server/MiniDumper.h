#ifndef MINIDUMPER_H
#define MINIDUMPER_H

#ifdef _WIN32
#include <windows.h>

class CMiniDumper
{
	public:

		CMiniDumper(bool bPromptUserForMiniDump);
		~CMiniDumper(void);

	private:

		static LONG WINAPI unhandledExceptionHandler(struct _EXCEPTION_POINTERS *pExceptionInfo);
		void setMiniDumpFileName(void);
		bool getImpersonationToken(HANDLE* phToken);
		BOOL enablePrivilege(LPCTSTR pszPriv, HANDLE hToken, TOKEN_PRIVILEGES* ptpOld);
		BOOL restorePrivilege(HANDLE hToken, TOKEN_PRIVILEGES* ptpOld);
		LONG writeMiniDump(_EXCEPTION_POINTERS *pExceptionInfo );

		_EXCEPTION_POINTERS *m_pExceptionInfo;
        char m_szMiniDumpPath[MAX_PATH];
        char m_szAppPath[MAX_PATH];
        char m_szAppBaseName[MAX_PATH];
		bool m_bPromptUserForMiniDump;

		static CMiniDumper* s_pMiniDumper;
		static LPCRITICAL_SECTION s_pCriticalSection;
};
#endif
#endif // MINIDUMPER_H
