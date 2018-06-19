#include "FrTrace.h"


#include <stdarg.h>     // va_list, va_start, va_arg, va_end 

#include <clocale> // std::setlocale

//this number is found empirically. I had a trace with 40000. this was too much : error at run time.
// 20000 was ok.
// so I decided 10000.
const int Logger::m_nbInstructionsPerFile = 10000;
int Logger::m_fileImportedCounter = 0;

#ifdef WIN32
#include <Windows.h> // for GetEnvironmentVariable
#else
#include <dirent.h> // for Linux...  and Mac?
#endif

#include "RprHybrid/Node/FrNode.h"

Logger::Logger() : m_type(TYPE_NONE)
{
	traceActivated = false;
	m_tracingPaused = 0;
	m_folderOfTraceFile = "";
	m_traceForcedByEnvVariable = false;

	m_recordFileIndex = 0;
	m_printfFileCount = 0;

	flog = NULL;
	flog_variables = NULL;
	flog_playList = NULL;
	flog_playerClassCpp = NULL;
	flog_playerClassH = NULL;


	const char envVariableName[] = "RPRTRACEPATH";

#ifdef WIN32
	char pathTrace[2048];
	pathTrace[0] = '\0';
	DWORD envVarRet = GetEnvironmentVariableA(envVariableName,pathTrace,sizeof(pathTrace)/sizeof(pathTrace[0]));
	if ( envVarRet != 0 && pathTrace[0] != '\0' && pathTrace[0] != '0' )
	{
		DWORD ftyp = GetFileAttributesA(pathTrace);
		if ( ftyp != INVALID_FILE_ATTRIBUTES && (ftyp & FILE_ATTRIBUTE_DIRECTORY) ) // if the directory is correct
		{
			SetTracingFolder(pathTrace);
		}

		//force trace.
		StartTrace();

		m_traceForcedByEnvVariable = true;
	}
#else


	// force trace with env variable : for Linux...  and Mac?

	char* pathTrace = getenv(envVariableName);
	if ( pathTrace != NULL && pathTrace[0] != '\0' && pathTrace[0] != '0' )
	{
		DIR* dir = opendir(pathTrace);
		if ( dir )
		{
			SetTracingFolder(pathTrace);
			closedir(dir);
		}

		//force trace.
		StartTrace();

		m_traceForcedByEnvVariable = true;
	}



#endif

}

Logger::~Logger()
{
	StopTrace();
}

void Logger::PauseTracing()
{
	if (traceActivated || m_tracingPaused > 0)
	{
		m_tracingPaused++;
		traceActivated = false;
	}
}

void Logger::ContinueTracing()
{
	if ( m_tracingPaused > 0 )
	{
		m_tracingPaused--;
		if ( m_tracingPaused == 0 )
		{
			traceActivated = true;
		}
	}
}

void Logger::SetTracingFolder(const char* tracingFolder)
{
	if ( m_traceForcedByEnvVariable ) { return; } // if trace forced, don't do this call

	m_folderOfTraceFile = std::string(tracingFolder);

	// add the folder separator at the end of the folder name
	if (   m_folderOfTraceFile.length() >= 1
		&& m_folderOfTraceFile[m_folderOfTraceFile.length()-1]  !=  '\\'
		&& m_folderOfTraceFile[m_folderOfTraceFile.length()-1]  !=  '/'
		)
	{
		m_folderOfTraceFile.push_back('/');
	}

	return;
}

void Logger::StartTrace()
{
	if (!traceActivated) // if was not already on
	{
		if ( m_traceForcedByEnvVariable ) { return; } // if trace forced, don't do this call

		traceActivated = true;
		if (flog) { fclose(flog); flog = NULL; }
		if (flog_variables) { fclose(flog_variables); flog_variables = NULL; }
		if (flog_playList) { fclose(flog_playList); flog_playList = NULL; }
		if (flog_playerClassCpp) { fclose(flog_playerClassCpp); flog_playerClassCpp = NULL; }
		if (flog_playerClassH) { fclose(flog_playerClassH); flog_playerClassH = NULL; }
		if (flog_data.is_open()) { flog_data.close(); }

		trace_rpr_framebuffer_format_count_created = 0;
		trace_rpr_framebuffer_format_count_used = 0;
		trace_rpr_context_properties_count_created = 0;
		trace_rpr_context_properties_count_used = 0;
		trace_rpr_framebuffer_desc_P_count_created = 0;
		trace_rpr_framebuffer_desc_P_count_used = 0;
		trace_rpr_image_desc_P_count_created = 0;
		trace_rpr_image_desc_P_count_used = 0;
		trace_rpr_buffer_desc_P_count_created = 0;
		trace_rpr_buffer_desc_P_count_used = 0;
		trace_rpr_float_P16_count_created = 0;
		trace_rpr_float_P16_count_used = 0;
		trace_rpr_image_format_count_created = 0;
		trace_rpr_image_format_count_used = 0;
		trace_rpr_tahoePluginIDlist_count_created = 0;
		trace_rpr_tahoePluginIDlist_count_used = 0;

		m_trace_countExportImage = 0;

		m_trace_addressToType.clear();
		m_mapContextToFramebuffer.clear();
		m_trace_listDeclaredFrObject.clear();

		cursorDataFile = 0;

		//std::string fullpath_cpp  = m_folderOfTraceFile + std::string("logFR.cpp");
		std::string fullpath_variables = m_folderOfTraceFile + std::string("rprTrace_variables.h");
		std::string fullpath_playList = m_folderOfTraceFile + std::string("rprTrace_playList.h");
		std::string fullpath_playerClassCpp = m_folderOfTraceFile + std::string("rprTrace_player.cpp");
		std::string fullpath_playerClassH = m_folderOfTraceFile + std::string("rprTrace_player.h");
		std::string fullpath_data = m_folderOfTraceFile + std::string("rprTrace_data.bin");

		

		//flog = fopen(fullpath_cpp.c_str(), "wb");
		flog_variables = fopen(fullpath_variables.c_str(), "wb");
		flog_playList = fopen(fullpath_playList.c_str(), "wb");
		flog_playerClassCpp = fopen(fullpath_playerClassCpp.c_str(), "wb");
		flog_playerClassH = fopen(fullpath_playerClassH.c_str(), "wb");
		flog_data.open(fullpath_data.c_str(), std::ios::trunc | std::ios::in | std::ios::out | std::ios::binary);

		if (   
			 flog_variables == NULL
			|| flog_playList == NULL
			|| flog_playerClassCpp == NULL
			|| flog_playerClassH == NULL
			|| flog_data.fail() || !flog_data.is_open()
			)
		{
			//if we can't open the trace file (maybe because read-only, or because not permission...etc... )
			//we disable tracing
			if (flog) { fclose(flog); flog = NULL; }
			if (flog_variables) { fclose(flog_variables); flog_variables = NULL; }
			if (flog_playList) { fclose(flog_playList); flog_playList = NULL; }
			if (flog_playerClassCpp) { fclose(flog_playerClassCpp); flog_playerClassCpp = NULL; }
			if (flog_playerClassH) { fclose(flog_playerClassH); flog_playerClassH = NULL; }
			if (flog_data.is_open()) { flog_data.close(); }
			traceActivated = false;
			return;
		}

		

		printVariable("//variables list\r\n");

		printPlaylist("//files list\r\n");
		printPlaylist("#undef RPRTRACINGMACRO__PLAYLIST\r\n");
		printPlaylist("#define RPRTRACINGMACRO__PLAYLIST ");

		printPlayerTraceH("#include \"rprTrace_playList.h\"\r\n");
		printPlayerTraceH("#include <fstream>\r\n");

		printPlayerTraceH("\r\n");
		printPlayerTraceH("#include <Rpr/RadeonProRender.h>\r\n");
		printPlayerTraceH("#include <Rpr/Math/matrix.h>\r\n");
		printPlayerTraceH("#include <Rpr/Math/mathutils.h>\r\n");
		printPlayerTraceH("\r\n");
		printPlayerTraceH("#define RPRTRACE_CHECK  CheckFrStatus(status);\r\n");
		printPlayerTraceH("#define RPRTRACE_DEV 1\r\n");
		printPlayerTraceH("\r\n");

		printPlayerTraceH("class RprTracePlayer\r\n");
		printPlayerTraceH("{\r\n");
		printPlayerTraceH("private:\r\n");
		printPlayerTraceH("\tstd::fstream dataFile;\r\n");
		printPlayerTraceH("\tvoid* pData1 = 0;\r\n");
		printPlayerTraceH("\tvoid* pData2 = 0;\r\n");
		printPlayerTraceH("\tvoid* pData3 = 0;\r\n");
		printPlayerTraceH("\tvoid* pData4 = 0;\r\n");
		printPlayerTraceH("\tvoid* pData5 = 0;\r\n");
		printPlayerTraceH("\tvoid* pData6 = 0;\r\n");
		printPlayerTraceH("\tvoid* pData7 = 0;\r\n");
		printPlayerTraceH("\tvoid* pData8 = 0;\r\n");
		printPlayerTraceH("\tvoid* pData9 = 0;\r\n");

		printPlayerTraceH("\tstd::vector<void*> ppData1;\r\n");
		printPlayerTraceH("\tstd::vector<void*> ppData2;\r\n");
		printPlayerTraceH("\tstd::vector<void*> ppData3;\r\n");
		printPlayerTraceH("\tstd::vector<void*> ppData4;\r\n");
		printPlayerTraceH("\tstd::vector<void*> ppData5;\r\n");
		printPlayerTraceH("\tstd::vector<void*> ppData6;\r\n");
		printPlayerTraceH("\tstd::vector<void*> ppData7;\r\n");
		printPlayerTraceH("\tstd::vector<void*> ppData8;\r\n");
		printPlayerTraceH("\tstd::vector<void*> ppData9;\r\n");

		printPlayerTraceH("\r\n");
		printPlayerTraceH("\trpr_int status = RPR_SUCCESS;\r\n\r\n");
		printPlayerTraceH("\t#include \"rprTrace_variables.h\"\r\n");
		printPlayerTraceH("\tvoid SeekFStream_64(std::fstream& s, unsigned long long pos);\r\n");
		printPlayerTraceH("\tvoid ClearPPData();\r\n");
		printPlayerTraceH("\tvoid CheckFrStatus(rpr_int status);\r\n");
		printPlayerTraceH("\t#define RPRTRACINGMACRO__PLAYLIST_BEG  int\r\n");
		printPlayerTraceH("\t#define RPRTRACINGMACRO__PLAYLIST_END  \r\n");
		printPlayerTraceH("\tRPRTRACINGMACRO__PLAYLIST;\r\n");
		printPlayerTraceH("\t#undef RPRTRACINGMACRO__PLAYLIST_BEG\r\n");
		printPlayerTraceH("\t#undef RPRTRACINGMACRO__PLAYLIST_END\r\n");
		printPlayerTraceH("public:\r\n");
		printPlayerTraceH("\trpr_int PlayAll();\r\n");
		printPlayerTraceH("\tvoid init();\r\n");
		printPlayerTraceH("};\r\n");

		SetupNextRecordPage();

		printPlayerTraceCpp("// Trace of Radeon ProRender API calls.\r\n");
		printPlayerTraceCpp("// API version of trace = 0x%p\r\n", RPR_API_VERSION);
		printPlayerTraceCpp("\r\n");



		printPlayerTraceCpp("#include \"rprTrace_player.h\"\r\n");
		printPlayerTraceCpp("#include \"rprTrace_playList.h\"\r\n");

		printPlayerTraceCpp("#define RPRTRACINGMACRO__PLAYLIST_BEG  status =\r\n");
		printPlayerTraceCpp("#define RPRTRACINGMACRO__PLAYLIST_END  CheckFrStatus(status);  if (status!=RPR_SUCCESS) {return status;} \r\n");
		printPlayerTraceCpp("rpr_int RprTracePlayer::PlayAll()\r\n");
		printPlayerTraceCpp("{\r\n");
		printPlayerTraceCpp("\tRPRTRACINGMACRO__PLAYLIST;\r\n");
		printPlayerTraceCpp("\treturn RPR_SUCCESS;\r\n");
		printPlayerTraceCpp("}\r\n");
		printPlayerTraceCpp("#undef RPRTRACINGMACRO__PLAYLIST_BEG\r\n");
		printPlayerTraceCpp("#undef RPRTRACINGMACRO__PLAYLIST_END\r\n");
		printPlayerTraceCpp("\r\n");
		printPlayerTraceCpp("void RprTracePlayer::init()\r\n");
		printPlayerTraceCpp("{\r\n");
		printPlayerTraceCpp("}\r\n");
		printPlayerTraceCpp("\r\n");

		printPlayerTraceCpp("void RprTracePlayer::SeekFStream_64(std::fstream& s, unsigned long long pos)\r\n");
		printPlayerTraceCpp("{\r\n");
		printPlayerTraceCpp("\tconst unsigned long long step = 0x010000000;\r\n");
		printPlayerTraceCpp("\ts.seekg(0, std::ios::beg);\r\n");
		printPlayerTraceCpp("\tunsigned long long nbStep = pos / (unsigned long long)step;\r\n");
		printPlayerTraceCpp("\tfor (unsigned long long i = 0; i < nbStep; i++)\r\n");
		printPlayerTraceCpp("\t{\r\n");
		printPlayerTraceCpp("\t\ts.seekg(step, std::ios::cur);\r\n");
		printPlayerTraceCpp("\t}\r\n");
		printPlayerTraceCpp("\tunsigned long long remainder_64 = pos %% (unsigned long long)step;\r\n");
		printPlayerTraceCpp("\tlong remainder_32 = remainder_64;\r\n");
		printPlayerTraceCpp("\ts.seekg(remainder_32, std::ios::cur);\r\n");
		printPlayerTraceCpp("}\r\n");
		printPlayerTraceCpp("\r\n");

		printPlayerTraceCpp("void RprTracePlayer::ClearPPData()\r\n");
		printPlayerTraceCpp("{\r\n");
		printPlayerTraceCpp("\tfor(int i=0; i < ppData1.size(); i++) { if ( ppData1[i] ) free(ppData1[i]); } ppData1.clear();\r\n");
		printPlayerTraceCpp("\tfor(int i=0; i < ppData2.size(); i++) { if ( ppData2[i] ) free(ppData2[i]); } ppData2.clear();\r\n");
		printPlayerTraceCpp("\tfor(int i=0; i < ppData3.size(); i++) { if ( ppData3[i] ) free(ppData3[i]); } ppData3.clear();\r\n");
		printPlayerTraceCpp("\tfor(int i=0; i < ppData4.size(); i++) { if ( ppData4[i] ) free(ppData4[i]); } ppData4.clear();\r\n");
		printPlayerTraceCpp("\tfor(int i=0; i < ppData5.size(); i++) { if ( ppData5[i] ) free(ppData5[i]); } ppData5.clear();\r\n");
		printPlayerTraceCpp("\tfor(int i=0; i < ppData6.size(); i++) { if ( ppData6[i] ) free(ppData6[i]); } ppData6.clear();\r\n");
		printPlayerTraceCpp("\tfor(int i=0; i < ppData7.size(); i++) { if ( ppData7[i] ) free(ppData7[i]); } ppData7.clear();\r\n");
		printPlayerTraceCpp("\tfor(int i=0; i < ppData8.size(); i++) { if ( ppData8[i] ) free(ppData8[i]); } ppData8.clear();\r\n");
		printPlayerTraceCpp("\tfor(int i=0; i < ppData9.size(); i++) { if ( ppData9[i] ) free(ppData9[i]); } ppData9.clear();\r\n");
		printPlayerTraceCpp("}\r\n");
		printPlayerTraceCpp("\r\n");

		printPlayerTraceCpp("\r\n");
		printPlayerTraceCpp("void RprTracePlayer::CheckFrStatus(rpr_int status)\r\n");
		printPlayerTraceCpp("{\r\n");
		printPlayerTraceCpp("\tif ( status != RPR_SUCCESS )\r\n");
		printPlayerTraceCpp("\t{\r\n");
		printPlayerTraceCpp("\t\t//manage error here\r\n");
		printPlayerTraceCpp("\t\tint a = 0;\r\n");
		printPlayerTraceCpp("\t}\r\n");
		printPlayerTraceCpp("}\r\n");
		printPlayerTraceCpp("\r\n");
		printPlayerTraceCpp("\r\n");

		printPlayerTraceCpp("RprTracePlayer player;\r\n");
		printPlayerTraceCpp("int main()\r\n");
		printPlayerTraceCpp("{\r\n");
		printPlayerTraceCpp("\tplayer.init();\r\n");
		printPlayerTraceCpp("\trpr_int retCode = player.PlayAll();\r\n");
		printPlayerTraceCpp("\treturn retCode;\r\n");
		printPlayerTraceCpp("}\r\n");


		
		printTrace("	dataFile.open(\"rprTrace_data.bin\", std::ios::in | std::ios::binary);\r\n");
		printTrace("	if ( dataFile.fail() || !dataFile.is_open() ) { CheckFrStatus(RPR_ERROR_INTERNAL_ERROR); return RPR_ERROR_INTERNAL_ERROR; }\r\n");


		

	}
}

void Logger::StopTrace()
{
	if (traceActivated) // if was not already off
	{
		if ( m_traceForcedByEnvVariable ) { return; } // if trace forced, don't do this call

		printTrace("	if ( pData1 ) { free(pData1); pData1=NULL; }\r\n");
		printTrace("	if ( pData2 ) { free(pData2); pData2=NULL; }\r\n");
		printTrace("	if ( pData3 ) { free(pData3); pData3=NULL; }\r\n");
		printTrace("	if ( pData4 ) { free(pData4); pData4=NULL; }\r\n");
		printTrace("	if ( pData5 ) { free(pData5); pData5=NULL; }\r\n");
		printTrace("	if ( pData6 ) { free(pData6); pData6=NULL; }\r\n");
		printTrace("	if ( pData7 ) { free(pData7); pData7=NULL; }\r\n");
		printTrace("	if ( pData8 ) { free(pData8); pData8=NULL; }\r\n");
		printTrace("	if ( pData9 ) { free(pData9); pData9=NULL; }\r\n");
		printTrace("	ClearPPData();\r\n");
		printTrace("	dataFile.close();\r\n");
		printTrace("	return RPR_SUCCESS;\r\n");
		printTrace("}\r\n");
		printTrace("\r\n//End of trace.\r\n\r\n");

		traceActivated = false;

		cursorDataFile = 0;

		if (flog) { fclose(flog); flog = NULL; }
		if (flog_variables) { fclose(flog_variables); flog_variables = NULL; }
		if (flog_playList) { fclose(flog_playList); flog_playList = NULL; }
		if (flog_playerClassCpp) { fclose(flog_playerClassCpp); flog_playerClassCpp = NULL; }
		if (flog_playerClassH) { fclose(flog_playerClassH); flog_playerClassH = NULL; }
		if (flog_data.is_open()) { flog_data.close(); }
	}
}

void Logger::setType(Type type)
{
	m_type = type;
}

void Logger::setLogPath(const char* path)
{

}

void Logger::printLog(const char* funcName, int n, ...)
{
	switch (m_type)
	{
	case TYPE_CONSOLE:
	{
		va_list args;
		va_start(args, n);
		printf("%s( ", funcName);
		for (int i = 0; i<n; i++)
		{
			rpr_longlong v = va_arg(args, rpr_longlong);
			printf("0x%llx ", v);
		}
		printf(")\n");
		va_end(args);
	}
		break;
	case TYPE_FILE:
		//todo. implement this
	default:
		break;
	};

}

void Logger::printTrace(const char* szfmt, ...)
{
	if (traceActivated)
	{
		va_list argptr;
		int32_t cnt;
		va_start(argptr, szfmt);
		cnt = vfprintf(flog, szfmt, argptr);
		va_end(argptr);
	}
}

void Logger::Trace__FlushAllFiles()
{
	if (traceActivated)
	{
		fflush(flog);
		fflush(flog_variables);
		fflush(flog_playList);
		fflush(flog_playerClassCpp);
		fflush(flog_playerClassH);
		flog_data.flush();
	}
}

void Logger::printVariable(const char* szfmt, ...)
{
	if (traceActivated)
	{
		va_list argptr;
		int32_t cnt;
		va_start(argptr, szfmt);
		cnt = vfprintf(flog_variables, szfmt, argptr);
		va_end(argptr);
		fflush(flog_variables);
	}
}

void Logger::printPlaylist(const char* szfmt, ...)
{
	if (traceActivated)
	{
		va_list argptr;
		int32_t cnt;
		va_start(argptr, szfmt);
		cnt = vfprintf(flog_playList, szfmt, argptr);
		va_end(argptr);
		fflush(flog_playList);
	}
}

void Logger::printPlayerTraceCpp(const char* szfmt, ...)
{
	if (traceActivated)
	{
		va_list argptr;
		int32_t cnt;
		va_start(argptr, szfmt);
		cnt = vfprintf(flog_playerClassCpp, szfmt, argptr);
		va_end(argptr);
		fflush(flog_playerClassCpp);
	}
}

void Logger::printPlayerTraceH(const char* szfmt, ...)
{
	if (traceActivated)
	{
		va_list argptr;
		int32_t cnt;
		va_start(argptr, szfmt);
		cnt = vfprintf(flog_playerClassH, szfmt, argptr);
		va_end(argptr);
		fflush(flog_playerClassH);
	}
}

void Logger::TraceArg__COMMA()
{
	if (traceActivated)
	{
		printTrace(",");
	}
}

void Logger::TraceArg__rpr_int(rpr_int a)
{
	if (traceActivated)
	{
		printTrace("(rpr_int)%d", a);
	}
}

void Logger::TraceArg__rpr_int_hexa(rpr_int a)
{
	if (traceActivated)
	{
		printTrace("(rpr_int)0x%p", a);
	}
}

void Logger::TraceArg__int(int a)
{
	if (traceActivated)
	{
		printTrace("(int)%d", a);
	}
}

void Logger::TraceArg__uint(unsigned int a)
{
	if (traceActivated)
	{
		printTrace("(unsigned int)%d", a);
	}
}

void Logger::TraceArg__rpr_uint(rpr_uint a)
{
	if (traceActivated)
	{
		printTrace("(rpr_uint)%d", a);
	}
}

void Logger::TraceArg__rpr_float(rpr_float a)
{
	if (traceActivated)
	{
		if (a == std::numeric_limits<float>::infinity())
		{
			printTrace("(rpr_float)std::numeric_limits<float>::infinity()");
		}
		else
		{
			std::string currentLocal = std::string( std::setlocale(LC_NUMERIC, NULL) ); // save current locale
			std::setlocale(LC_NUMERIC, "C"); // set minimal local so that the decimal separator is a dot

			printTrace("(rpr_float)%ff", a);

			std::setlocale(LC_NUMERIC, currentLocal.c_str()); // restore current locale
		}
	}
}

void Logger::TraceArg__rpr_bool(rpr_bool a)
{
	if (traceActivated)
	{
		if (a) { printTrace("true"); }
		else { printTrace("false"); }
	}
}

void Logger::TraceArg__size_t(size_t a)
{
	if (traceActivated)
	{
		printTrace("(size_t)%d", a);
	}
}

void Logger::TraceArg__rpr_hetero_volume_indices_topology(rpr_hetero_volume_indices_topology a)
{
	if (traceActivated)
	{
			 if (a == RPR_HETEROVOLUME_INDICES_TOPOLOGY_I_U64) { printTrace("RPR_HETEROVOLUME_INDICES_TOPOLOGY_I_U64"); }
		else if (a == RPR_HETEROVOLUME_INDICES_TOPOLOGY_XYZ_U32) { printTrace("RPR_HETEROVOLUME_INDICES_TOPOLOGY_XYZ_U32"); }
		else if (a == RPR_HETEROVOLUME_INDICES_TOPOLOGY_I_S64) { printTrace("RPR_HETEROVOLUME_INDICES_TOPOLOGY_I_S64"); }
		else if (a == RPR_HETEROVOLUME_INDICES_TOPOLOGY_XYZ_S32) { printTrace("RPR_HETEROVOLUME_INDICES_TOPOLOGY_XYZ_S32"); }
		else { printTrace("?%d?", a); }
	}

}

void Logger::TraceArg__rpr_char_P(const rpr_char* a)
{
	if (traceActivated)
	{
		if (a == NULL)
		{
			printTrace("(rpr_char*)NULL");
		}
		else
		{

			printTrace("(rpr_char*)\"");
			for (int iChar = 0;; iChar++)
			{
				if (a[iChar] == '\0')
				{
					break;
				}

				else if  (a[iChar] == '\r')
				{
					char charToStr3[3];
					charToStr3[0] = '\\';
					charToStr3[1] = 'r';
					charToStr3[2] = '\0';
					printTrace(charToStr3);
				}

				else if  (a[iChar] == '\n')
				{
					char charToStr3[3];
					charToStr3[0] = '\\';
					charToStr3[1] = 'n';
					charToStr3[2] = '\0';
					printTrace(charToStr3);
				}

				else if  (a[iChar] == '\"')
				{
					char charToStr3[3];
					charToStr3[0] = '\\';
					charToStr3[1] = '\"';
					charToStr3[2] = '\0';
					printTrace(charToStr3);
				}

				else
				{

					char charToStr[2];
					charToStr[0] = a[iChar];
					charToStr[1] = '\0';
					printTrace(charToStr); //don't know why, but this doesn't work on Linux:  printTrace("%c", a[iChar]);

					if (a[iChar] == '\\') 
					{
						printTrace("\\");
					}

				}


			}
			printTrace("\"");
		}
	}
}


void Logger::TraceArg__STATUS()
{
	if (traceActivated)
	{
		printTrace("(rpr_int*)&status");
	}
}


void Logger::TraceArg__rpr_context(rpr_context a)						{ if (traceActivated) { if (a) { printTrace("context_0x%p", a); }			else { printTrace("(rpr_context)NULL"); } } }
void Logger::TraceArg__rpr_post_effect(rpr_post_effect a)				{ if (traceActivated) { if (a) { printTrace("posteffect_0x%p", a); }		else { printTrace("(rpr_post_effect)NULL"); } } }
void Logger::TraceArg__rpr_camera(rpr_camera a)							{ if (traceActivated) { if (a) { printTrace("camera_0x%p", a); }			else { printTrace("(rpr_camera)NULL"); } } }
void Logger::TraceArg__rpr_shape(rpr_shape a)							{ if (traceActivated) { if (a) { printTrace("shape_0x%p", a); }				else { printTrace("(rpr_shape)NULL"); } } }
void Logger::TraceArg__rpr_light(rpr_light a)							{ if (traceActivated) { if (a) { printTrace("light_0x%p", a); }				else { printTrace("(rpr_light)NULL"); } } }
void Logger::TraceArg__rpr_scene(rpr_scene a)							{ if (traceActivated) { if (a) { printTrace("scene_0x%p", a); }				else { printTrace("(rpr_scene)NULL"); } } }
void Logger::TraceArg__rpr_image(rpr_image a)							{ if (traceActivated) { if (a) { printTrace("image_0x%p", a); }				else { printTrace("(rpr_image)NULL"); } } }
void Logger::TraceArg__rpr_buffer(rpr_buffer a)							{ if (traceActivated) { if (a) { printTrace("buffer_0x%p", a); }			else { printTrace("(rpr_buffer)NULL"); } } }
void Logger::TraceArg__rpr_framebuffer(rpr_framebuffer a)				{ if (traceActivated) { if (a) { printTrace("framebuffer_0x%p", a); }		else { printTrace("(rpr_framebuffer)NULL"); } } }
void Logger::TraceArg__rpr_hetero_volume(rpr_hetero_volume a)			{ if (traceActivated) { if (a) { printTrace("heterovolume_0x%p", a); }		else { printTrace("(rpr_hetero_volume)NULL"); } } }
void Logger::TraceArg__rpr_material_node(rpr_material_node a)			{ if (traceActivated) { if (a) { printTrace("materialnode_0x%p", a); }		else { printTrace("(rpr_material_node)NULL"); } } }
void Logger::TraceArg__rpr_composite(rpr_composite a)					{ if (traceActivated) { if (a) { printTrace("composite_0x%p", a); }			else { printTrace("(rpr_composite)NULL"); } } }
void Logger::TraceArg__rpr_lut(rpr_lut a)								{ if (traceActivated) { if (a) { printTrace("lut_0x%p", a); }				else { printTrace("(rpr_lut)NULL"); } } }
void Logger::TraceArg__rpr_material_system(rpr_material_system a)		{ if (traceActivated) { if (a) { printTrace("materialsystem_0x%p", a); }	else { printTrace("(rpr_material_system)NULL"); } } }
void Logger::TraceArg__rpr_tahoePluginID(rpr_int a)						{ if (traceActivated) { if (a) { printTrace("tahoePluginID_0x%p", a); }		else { printTrace("tahoePluginID_0x0000"); } } } // for tahoePluginID, we can have 0.  (reminder : linux replaces 0 address by "(nil)" )


void Logger::TraceArg__rpr_undef(const void* a)
{
	if (traceActivated)
	{
		if ( a == NULL )
		{
			printTrace("NULL");
		}
		else if (m_trace_addressToType.find(a) != m_trace_addressToType.end())
		{
			TRACE_ADDRESS_TYPE type = m_trace_addressToType[a];
			if (type == TRACE_ADDRESS_TYPE__LIGHT)					{ printTrace("light_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__IMAGE)				{ printTrace("image_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__BUFFER)			{ printTrace("buffer_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__CAMERA)			{ printTrace("camera_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__FRAMEBUFFER)		{ printTrace("framebuffer_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__SCENE)				{ printTrace("scene_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__SHAPE)				{ printTrace("shape_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__MATERIALSYSTEM)	{ printTrace("materialsystem_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__MATERIALNODE)		{ printTrace("materialnode_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__CONTEXT)			{ printTrace("context_0x%p", a); }
			//else if (type == TRACE_ADDRESS_TYPE__TAHOEPLUGINID)   { printTrace("tahoePluginID_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__POSTEFFECT)		{ printTrace("posteffect_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__COMPOSITE)			{ printTrace("composite_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__LUT)				{ printTrace("lut_0x%p", a); }
			else if (type == TRACE_ADDRESS_TYPE__HETEROVOLUME)		{ printTrace("heterovolume_0x%p", a); }
			else { printTrace("(???)???_0x%p", a); }
		}
		else
		{
			printTrace("(????)????_0x%p", a);
		}
	}
}

void Logger::TraceArg_Prepare__rpr_framebuffer_format(rpr_framebuffer_format a)
{
	if (traceActivated)
	{
		printTrace("rpr_framebuffer_format framebuffer_format%d = { %d, \r\n", trace_rpr_framebuffer_format_count_created, a.num_components);
		
		if (a.type == RPR_COMPONENT_TYPE_UINT8)
		{
			printTrace("RPR_COMPONENT_TYPE_UINT8");
		}
		else if (a.type == RPR_COMPONENT_TYPE_FLOAT16)
		{
			printTrace("RPR_COMPONENT_TYPE_FLOAT16");
		}
		else if (a.type == RPR_COMPONENT_TYPE_FLOAT32)
		{
			printTrace("RPR_COMPONENT_TYPE_FLOAT32");
		}
		else
		{
			printTrace("?%d?", a.type);
		}
		printTrace(" };\r\n");

		trace_rpr_framebuffer_format_count_created++;
	}
}
void Logger::TraceArg_Use__rpr_framebuffer_format(rpr_framebuffer_format a)
{
	if (traceActivated)
	{
		printTrace("(rpr_framebuffer_format)framebuffer_format%d", trace_rpr_framebuffer_format_count_used);
		trace_rpr_framebuffer_format_count_used++;
	}
}



void Logger::TraceArg_Prepare__rpr_context_properties_P(const rpr_context_properties* a)
{
	if (traceActivated)
	{
		if ( a != nullptr )
		{
			int nbElement=0;
			for(; ; nbElement++)
			{
				if ( a[nbElement] == 0 )
				{
					break;
				}
				else
				{
					nbElement++;
				}
			}

			nbElement++; // add the '0' 

			printTrace("rpr_context_properties context_properties%d[%d];\r\n", trace_rpr_context_properties_count_created , nbElement);

			for(int i=0; i<nbElement; i++)
			{
				printTrace("rpr_context_properties context_properties%d[%d] =  (rpr_context_properties)%d ;\r\n", trace_rpr_context_properties_count_created, i , (rpr_context_properties)a[i] );
			}

			trace_rpr_context_properties_count_created++;
		}
	}
}
void Logger::TraceArg_Use__rpr_context_properties_P(const rpr_context_properties* a)
{
	if (traceActivated)
	{
		if ( a != nullptr )
		{
			printTrace("(rpr_context_properties*)context_properties%d", trace_rpr_context_properties_count_used);
			trace_rpr_context_properties_count_used++;
		}
		else
		{
			printTrace("(rpr_context_properties*)0");
		}
	}
}





void Logger::TraceArg_Prepare__rpr_mesh_info_P(const rpr_mesh_info* a)
{
	if (traceActivated)
	{
		if ( a != nullptr )
		{
			int nbElement=0;
			for(; ; nbElement++)
			{
				if ( a[nbElement] == 0 )
				{
					break;
				}
				else
				{
					nbElement++;
				}
			}

			nbElement++; // add the '0' 

			printTrace("rpr_mesh_info mesh_info%d[%d];\r\n", trace_rpr_mesh_info_count_created , nbElement);

			for(int i=0; i<nbElement; i++)
			{
				printTrace("mesh_info%d[%d] =  (rpr_mesh_info)%d ;\r\n", trace_rpr_mesh_info_count_created, i , (rpr_mesh_info)a[i] );
			}

			trace_rpr_mesh_info_count_created++;
		}
	}
}
void Logger::TraceArg_Use__rpr_mesh_info_P(const rpr_mesh_info* a)
{
	if (traceActivated)
	{
		if ( a != nullptr )
		{
			printTrace("(rpr_mesh_info*)mesh_info%d", trace_rpr_mesh_info_count_used);
			trace_rpr_mesh_info_count_used++;
		}
		else
		{
			printTrace("(rpr_mesh_info*)0");
		}
	}
}







void Logger::TraceArg_Prepare__rpr_image_format(rpr_image_format a)
{
	if (traceActivated)
	{
		printTrace("rpr_image_format image_format%d = { %d, ", trace_rpr_image_format_count_created, a.num_components);
		if (a.type == RPR_COMPONENT_TYPE_UINT8)
		{
			printTrace("RPR_COMPONENT_TYPE_UINT8");
		}
		else if (a.type == RPR_COMPONENT_TYPE_FLOAT16)
		{
			printTrace("RPR_COMPONENT_TYPE_FLOAT16");
		}
		else if (a.type == RPR_COMPONENT_TYPE_FLOAT32)
		{
			printTrace("RPR_COMPONENT_TYPE_FLOAT32");
		}
		else
		{
			printTrace("?%d?", a.type);
		}
		printTrace(" };\r\n");
		trace_rpr_image_format_count_created++;
		
	}
}

void Logger::TraceArg_Use__rpr_image_format(rpr_image_format a)
{
	if (traceActivated)
	{
		printTrace("(rpr_image_format)image_format%d", trace_rpr_image_format_count_used);
		trace_rpr_image_format_count_used++;
	}
}


void Logger::TraceArg_Prepare__rpr_framebuffer_desc_P(const rpr_framebuffer_desc* a)
{
	if (traceActivated)
	{
		if ( a )
		{
			printTrace("rpr_framebuffer_desc framebuffer_desc%d = { %d, %d };\r\n", trace_rpr_framebuffer_desc_P_count_created, a->fb_width, a->fb_height);
			trace_rpr_framebuffer_desc_P_count_created++;
		}
	}
}
void Logger::TraceArg_Use__rpr_framebuffer_desc_P(const rpr_framebuffer_desc* a)
{
	if (traceActivated)
	{
		if ( a )
		{
			printTrace("(rpr_framebuffer_desc*)&framebuffer_desc%d", trace_rpr_framebuffer_desc_P_count_used);
			trace_rpr_framebuffer_desc_P_count_used++;
		}
		else
		{
			printTrace("(rpr_framebuffer_desc*)0");
		}
	}
}


void Logger::TraceArg_Prepare__rpr_image_desc_P(const rpr_image_desc* a)
{
	if (traceActivated)
	{
		if ( a )
		{
			printTrace("rpr_image_desc image_desc%d = { %d, %d, %d, %d, %d };\r\n", trace_rpr_image_desc_P_count_created, a->image_width, a->image_height, a->image_depth, a->image_row_pitch, a->image_slice_pitch);
			trace_rpr_image_desc_P_count_created++;
		}
	}
}
void Logger::TraceArg_Use__rpr_image_desc_P(const rpr_image_desc* a)
{
	if (traceActivated)
	{
		if ( a )
		{
			printTrace("(rpr_image_desc*)&image_desc%d", trace_rpr_image_desc_P_count_used);
			trace_rpr_image_desc_P_count_used++;
		}
		else
		{
			printTrace("(rpr_image_desc*)0");
		}
	}
}

void Logger::TraceArg_Prepare__rpr_buffer_desc_P(const rpr_buffer_desc* a)
{
	if (traceActivated)
	{
		if ( a )
		{
			std::string elementTypeStr;
				 if ( a->element_type == RPR_BUFFER_ELEMENT_TYPE_INT32 )	 { elementTypeStr = "RPR_BUFFER_ELEMENT_TYPE_INT32";  }
			else if ( a->element_type == RPR_BUFFER_ELEMENT_TYPE_FLOAT32 )	 { elementTypeStr = "RPR_BUFFER_ELEMENT_TYPE_FLOAT32";  }
			else { elementTypeStr = "???"; }

			printTrace("rpr_buffer_desc buffer_desc%d = { %d, %s, %d };\r\n", 
				trace_rpr_buffer_desc_P_count_created, 
				a->nb_element, 
				elementTypeStr.c_str(), 
				a->element_channel_size
				);
			trace_rpr_buffer_desc_P_count_created++;
		}
	}
}
void Logger::TraceArg_Use__rpr_buffer_desc_P(const rpr_buffer_desc* a)
{
	if (traceActivated)
	{
		if ( a )
		{
			printTrace("(rpr_buffer_desc*)&buffer_desc%d", trace_rpr_buffer_desc_P_count_used);
			trace_rpr_buffer_desc_P_count_used++;
		}
		else
		{
			printTrace("(rpr_buffer_desc*)0");
		}
	}
}



void Logger::TraceArg_Prepare__rpr_tahoePluginIDlist(const rpr_int* a, size_t numberOfId)
{
	if (traceActivated)
	{
		printTrace("rpr_int tahoePluginIDlist_%d[%d] = { ", trace_rpr_tahoePluginIDlist_count_created, numberOfId);

		for(size_t i=0; i<numberOfId; i++)
		{
			TraceArg__rpr_tahoePluginID(a[i]); 
			if ( i != numberOfId-1 ) { printTrace(","); }
		}

		printTrace("};\r\n");
		trace_rpr_tahoePluginIDlist_count_created++;
	}
}

void Logger::TraceArg_Use__rpr_tahoePluginIDlist(const rpr_int* a)
{
	if (traceActivated)
	{
		printTrace("(rpr_int*)&tahoePluginIDlist_%d", trace_rpr_tahoePluginIDlist_count_used); trace_rpr_tahoePluginIDlist_count_used++;
	}
}

void Logger::TraceArg_Prepare__rpr_float_P16(const rpr_float* a)
{
	if (traceActivated)
	{
		std::string currentLocal = std::string( std::setlocale(LC_NUMERIC, NULL) ); // save current locale
		std::setlocale(LC_NUMERIC, "C"); // set minimal local so that the decimal separator is a dot

		printTrace("rpr_float float_P16_%d[] = { %ff,%ff,%ff,%ff,%ff,%ff,%ff,%ff,%ff,%ff,%ff,%ff,%ff,%ff,%ff,%ff };\r\n", trace_rpr_float_P16_count_created, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]);
		
		std::setlocale(LC_NUMERIC, currentLocal.c_str()); // restore current locale
		
		trace_rpr_float_P16_count_created++;
	}
}
void Logger::TraceArg_Use__rpr_float_P16(const rpr_float* a)
{
	if (traceActivated)
	{
		printTrace("(rpr_float*)&float_P16_%d", trace_rpr_float_P16_count_used); trace_rpr_float_P16_count_used++;
	}
}


void Logger::TraceArg_Prepare__DATA(const void* data, unsigned long long dataSize, const char* varName)
{
	if (traceActivated)
	{
		if (data != NULL && dataSize != 0 ) 
		{
			unsigned long long dataPosition = cursorDataFile;
			flog_data.write((const char*)data, dataSize);
			//fwrite(data,1, dataSize, flog_data);
			cursorDataFile += dataSize;

			printTrace("%s = realloc(%s,%llu);\r\n", varName, varName, dataSize);
			printTrace("if ( %s == NULL ) { CheckFrStatus(RPR_ERROR_INTERNAL_ERROR); return RPR_ERROR_INTERNAL_ERROR; }\r\n", varName);
			printTrace("SeekFStream_64(dataFile, %llu);\r\n", dataPosition);
			printTrace("dataFile.read((char*)%s, %llu);\r\n", varName, dataSize);
		}
		else if ( data != NULL && dataSize == 0 )
		{
			printTrace("// WARNING : %s has no size.\r\n",varName);
			printTrace("if ( %s ) { free(%s); %s=NULL; }\r\n",varName,varName,varName);
		}
		else
		{
			printTrace("// %s not used - null argument.\r\n",varName);
			printTrace("if ( %s ) { free(%s); %s=NULL; }\r\n",varName,varName,varName);
		}
	}
}

void Logger::TraceArg_Prepare__PDATA(const void** data, unsigned long long* dataSize, int numberOfData, const char* varName)
{
	if (traceActivated)
	{
		//clear the old one
		printTrace("for(int i=0; i < %s.size(); i++) { if ( %s[i] ) free(%s[i]); } %s.clear();\r\n",varName,varName,varName,varName);
		
		if (data != NULL && dataSize != 0 ) 
		{
			printTrace("%s.assign(%d,nullptr);\r\n",varName,numberOfData);

			for(int iData=0; iData<numberOfData; iData++)
			{

				if ( data[iData] != nullptr && dataSize[iData] != 0 )
				{

					unsigned long long dataPosition = cursorDataFile;
					flog_data.write((const char*)data[iData], dataSize[iData]);
					cursorDataFile += dataSize[iData];

					printTrace("%s[%d] = malloc(%llu);\r\n", varName,iData, dataSize[iData]);
					printTrace("if ( %s[%d] == NULL ) { CheckFrStatus(RPR_ERROR_INTERNAL_ERROR); return RPR_ERROR_INTERNAL_ERROR; }\r\n", varName,iData);
					printTrace("SeekFStream_64(dataFile, %llu);\r\n", dataPosition);
					printTrace("dataFile.read((char*)%s[%d], %llu);\r\n", varName,iData, dataSize[iData]);

				}
				else
				{
					printTrace("%s[%d] = NULL;\r\n", varName,iData);
				}

			}

		}
		else if ( data != NULL && dataSize == 0 )
		{
			printTrace("// WARNING : %s has no size.\r\n",varName);
		}
		else
		{
			printTrace("// %s not used - null argument.\r\n",varName);
		}
	}
}

void Logger::TraceArg_Use__DATA_const_rpr_float_P(const char* varName)
{
	if (traceActivated)
	{
		if (varName != NULL)
		{
			printTrace("(const rpr_float*)(%s)", varName);
		}
		else
		{
			printTrace("(const rpr_float*)(???)"); // should not happen
		}
	}
}

void Logger::TraceArg_Use__DATA_const_rpr_float_PP(const char* varName)
{
	if (traceActivated)
	{
		if (varName != NULL)
		{
			printTrace("(const rpr_float**)(%s)", varName);
		}
		else
		{
			printTrace("(const rpr_float**)(???)"); // should not happen
		}
	}
}

void Logger::TraceArg_Use__DATA_const_rpr_int_P(const char* varName)
{
	if (traceActivated)
	{
		if (varName != NULL)
		{
			printTrace("(const rpr_int*)(%s)", varName);
		}
		else
		{
			printTrace("(const rpr_int*)(???)"); // should not happen
		}
	}
}
void Logger::TraceArg_Use__DATA_const_rpr_int_PP(const char* varName)
{
	if (traceActivated)
	{
		if (varName != NULL)
		{
			printTrace("(const rpr_int**)(%s)", varName);
		}
		else
		{
			printTrace("(const rpr_int**)(???)"); // should not happen
		}
	}
}
void Logger::TraceArg_Use__DATA_const_size_t_P(const char* varName)
{
	if (traceActivated)
	{
		if (varName != NULL)
		{
			printTrace("(const size_t*)(%s)", varName);
		}
		else
		{
			printTrace("(const size_t*)(???)"); // should not happen
		}
	}
}
void Logger::TraceArg_Use__DATA_const_void_P(const char* varName)
{
	if (traceActivated)
	{
		if (varName != NULL)
		{
			printTrace("(const void*)(%s)", varName);
		}
		else
		{
			printTrace("(const void*)(???)"); // should not happen
		}

	}
}

void Logger::TraceArg__rpr_environment_override(rpr_environment_override a)
{
	if (traceActivated)
	{
			 if (a == RPR_SCENE_ENVIRONMENT_OVERRIDE_REFLECTION) { printTrace("RPR_SCENE_ENVIRONMENT_OVERRIDE_REFLECTION"); }
		else if (a == RPR_SCENE_ENVIRONMENT_OVERRIDE_REFRACTION) { printTrace("RPR_SCENE_ENVIRONMENT_OVERRIDE_REFRACTION"); }
		else if (a == RPR_SCENE_ENVIRONMENT_OVERRIDE_TRANSPARENCY) { printTrace("RPR_SCENE_ENVIRONMENT_OVERRIDE_TRANSPARENCY"); }
		else if (a == RPR_SCENE_ENVIRONMENT_OVERRIDE_BACKGROUND) { printTrace("RPR_SCENE_ENVIRONMENT_OVERRIDE_BACKGROUND"); }
		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_subdiv_boundary_interfop_type(rpr_subdiv_boundary_interfop_type a)
{
	if (traceActivated)
	{
			 if (a == RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_AND_CORNER) { printTrace("RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_AND_CORNER"); }
		else if (a == RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_ONLY) { printTrace("RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_ONLY"); }
		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_image_wrap_type(rpr_image_wrap_type a)
{
	if (traceActivated)
	{
			 if (a == RPR_IMAGE_WRAP_TYPE_REPEAT) { printTrace("RPR_IMAGE_WRAP_TYPE_REPEAT"); }
		else if (a == RPR_IMAGE_WRAP_TYPE_MIRRORED_REPEAT) { printTrace("RPR_IMAGE_WRAP_TYPE_MIRRORED_REPEAT"); }
		else if (a == RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE) { printTrace("RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE"); }
		else if (a == RPR_IMAGE_WRAP_TYPE_CLAMP_TO_BORDER) { printTrace("RPR_IMAGE_WRAP_TYPE_CLAMP_TO_BORDER"); }
		else if (a == RPR_IMAGE_WRAP_TYPE_CLAMP_ZERO) { printTrace("RPR_IMAGE_WRAP_TYPE_CLAMP_ZERO"); }
		else if (a == RPR_IMAGE_WRAP_TYPE_CLAMP_ONE) { printTrace("RPR_IMAGE_WRAP_TYPE_CLAMP_ONE"); }
		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_hetero_volume_filter(rpr_hetero_volume_filter a)
{
	if (traceActivated)
	{
			 if (a == RPR_HETEROVOLUME_FILTER_NEAREST) { printTrace("RPR_HETEROVOLUME_FILTER_NEAREST"); }
		else if (a == RPR_HETEROVOLUME_FILTER_LINEAR) { printTrace("RPR_HETEROVOLUME_FILTER_LINEAR"); }
		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_image_filter_type(rpr_image_filter_type a)
{
	if (traceActivated)
	{
			 if (a == RPR_IMAGE_FILTER_TYPE_NEAREST) { printTrace("RPR_IMAGE_FILTER_TYPE_NEAREST"); }
		else if (a == RPR_IMAGE_FILTER_TYPE_LINEAR) { printTrace("RPR_IMAGE_FILTER_TYPE_LINEAR"); }
		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_material_system_type(rpr_material_system_type a)
{
	if (traceActivated)
	{
		printTrace("%d", a);
	}
}

void Logger::TraceArg__rpr_camera_mode(rpr_camera_mode a)
{
	if (traceActivated)
	{
		if (a == RPR_CAMERA_MODE_PERSPECTIVE) { printTrace("RPR_CAMERA_MODE_PERSPECTIVE"); }
		else if (a == RPR_CAMERA_MODE_ORTHOGRAPHIC) { printTrace("RPR_CAMERA_MODE_ORTHOGRAPHIC"); }
		else if (a == RPR_CAMERA_MODE_LATITUDE_LONGITUDE_360) { printTrace("RPR_CAMERA_MODE_LATITUDE_LONGITUDE_360"); }
		else if (a == RPR_CAMERA_MODE_LATITUDE_LONGITUDE_STEREO) { printTrace("RPR_CAMERA_MODE_LATITUDE_LONGITUDE_STEREO"); }
		else if (a == RPR_CAMERA_MODE_CUBEMAP) { printTrace("RPR_CAMERA_MODE_CUBEMAP"); }
		else if (a == RPR_CAMERA_MODE_CUBEMAP_STEREO) { printTrace("RPR_CAMERA_MODE_CUBEMAP_STEREO"); }
		else if (a == RPR_CAMERA_MODE_FISHEYE) { printTrace("RPR_CAMERA_MODE_FISHEYE"); }
		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_creation_flags(rpr_creation_flags a)
{
	if (traceActivated)
	{
		if (a & RPR_CREATION_FLAGS_ENABLE_GPU0) { printTrace("RPR_CREATION_FLAGS_ENABLE_GPU0 | "); }
		if (a & RPR_CREATION_FLAGS_ENABLE_GPU1) { printTrace("RPR_CREATION_FLAGS_ENABLE_GPU1 | "); }
		if (a & RPR_CREATION_FLAGS_ENABLE_GPU2) { printTrace("RPR_CREATION_FLAGS_ENABLE_GPU2 | "); }
		if (a & RPR_CREATION_FLAGS_ENABLE_GPU3) { printTrace("RPR_CREATION_FLAGS_ENABLE_GPU3 | "); }
		if (a & RPR_CREATION_FLAGS_ENABLE_GPU4) { printTrace("RPR_CREATION_FLAGS_ENABLE_GPU4 | "); }
		if (a & RPR_CREATION_FLAGS_ENABLE_GPU5) { printTrace("RPR_CREATION_FLAGS_ENABLE_GPU5 | "); }
		if (a & RPR_CREATION_FLAGS_ENABLE_GPU6) { printTrace("RPR_CREATION_FLAGS_ENABLE_GPU6 | "); }
		if (a & RPR_CREATION_FLAGS_ENABLE_GPU7) { printTrace("RPR_CREATION_FLAGS_ENABLE_GPU7 | "); }
		if (a & RPR_CREATION_FLAGS_ENABLE_CPU) { printTrace("RPR_CREATION_FLAGS_ENABLE_CPU | "); }
		if (a & RPR_CREATION_FLAGS_ENABLE_GL_INTEROP) { printTrace("RPR_CREATION_FLAGS_ENABLE_GL_INTEROP | "); }
		printTrace("0");
	}
}

void Logger::TraceArg__rpr_render_mode(rpr_render_mode a)
{
	if (traceActivated)
	{
		if (a == RPR_RENDER_MODE_GLOBAL_ILLUMINATION) { printTrace("RPR_RENDER_MODE_GLOBAL_ILLUMINATION"); }
		else if (a == RPR_RENDER_MODE_DIRECT_ILLUMINATION) { printTrace("RPR_RENDER_MODE_DIRECT_ILLUMINATION"); }
		else if (a == RPR_RENDER_MODE_DIRECT_ILLUMINATION_NO_SHADOW) { printTrace("RPR_RENDER_MODE_DIRECT_ILLUMINATION_NO_SHADOW"); }
		else if (a == RPR_RENDER_MODE_WIREFRAME) { printTrace("RPR_RENDER_MODE_WIREFRAME"); }
		else if (a == RPR_RENDER_MODE_MATERIAL_INDEX) { printTrace("RPR_RENDER_MODE_MATERIAL_INDEX"); }
		else if (a == RPR_RENDER_MODE_POSITION) { printTrace("RPR_RENDER_MODE_POSITION"); }
		else if (a == RPR_RENDER_MODE_NORMAL) { printTrace("RPR_RENDER_MODE_NORMAL"); }
		else if (a == RPR_RENDER_MODE_TEXCOORD) { printTrace("RPR_RENDER_MODE_TEXCOORD"); }
		else if (a == RPR_RENDER_MODE_AMBIENT_OCCLUSION) { printTrace("RPR_RENDER_MODE_AMBIENT_OCCLUSION"); }
		else if (a == RPR_RENDER_MODE_DIFFUSE) { printTrace("RPR_RENDER_MODE_DIFFUSE"); }
		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_mesh_polygon_vertex_info(rpr_mesh_polygon_vertex_info a)
{
	if (traceActivated)
	{
		if (a == RPR_MESH_POLYGON_VERTEX_POSITION) { printTrace("RPR_MESH_POLYGON_VERTEX_POSITION"); }
		else if (a == RPR_MESH_POLYGON_VERTEX_NORMAL) { printTrace("RPR_MESH_POLYGON_VERTEX_NORMAL"); }
		else if (a == RPR_MESH_POLYGON_VERTEX_TEXCOORD) { printTrace("RPR_MESH_POLYGON_VERTEX_TEXCOORD"); }
		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_post_effect_type(rpr_post_effect_type a)
{
	if (traceActivated)
	{
			 if (a == RPR_POST_EFFECT_WHITE_BALANCE) { printTrace("RPR_POST_EFFECT_WHITE_BALANCE"); }
		else if (a == RPR_POST_EFFECT_SIMPLE_TONEMAP) { printTrace("RPR_POST_EFFECT_SIMPLE_TONEMAP"); }
		else if (a == RPR_POST_EFFECT_TONE_MAP) { printTrace("RPR_POST_EFFECT_TONE_MAP"); }
		else if (a == RPR_POST_EFFECT_NORMALIZATION) { printTrace("RPR_POST_EFFECT_NORMALIZATION"); }
		else if (a == RPR_POST_EFFECT_GAMMA_CORRECTION) { printTrace("RPR_POST_EFFECT_GAMMA_CORRECTION"); }
		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_material_node_arithmetic_operation(rpr_material_node_arithmetic_operation a)
{
	if (traceActivated)
	{
			 if (a == RPR_MATERIAL_NODE_OP_ADD) { printTrace("RPR_MATERIAL_NODE_OP_ADD"); }
		else if (a == RPR_MATERIAL_NODE_OP_SUB) { printTrace("RPR_MATERIAL_NODE_OP_SUB"); }
		else if (a == RPR_MATERIAL_NODE_OP_MUL) { printTrace("RPR_MATERIAL_NODE_OP_MUL"); }
		else if (a == RPR_MATERIAL_NODE_OP_DIV) { printTrace("RPR_MATERIAL_NODE_OP_DIV"); }
		else if (a == RPR_MATERIAL_NODE_OP_SIN) { printTrace("RPR_MATERIAL_NODE_OP_SIN"); }
		else if (a == RPR_MATERIAL_NODE_OP_COS) { printTrace("RPR_MATERIAL_NODE_OP_COS"); }
		else if (a == RPR_MATERIAL_NODE_OP_TAN) { printTrace("RPR_MATERIAL_NODE_OP_TAN"); }
		else if (a == RPR_MATERIAL_NODE_OP_SELECT_X) { printTrace("RPR_MATERIAL_NODE_OP_SELECT_X"); }
		else if (a == RPR_MATERIAL_NODE_OP_SELECT_Y) { printTrace("RPR_MATERIAL_NODE_OP_SELECT_Y"); }
		else if (a == RPR_MATERIAL_NODE_OP_SELECT_Z) { printTrace("RPR_MATERIAL_NODE_OP_SELECT_Z"); }
		else if (a == RPR_MATERIAL_NODE_OP_COMBINE) { printTrace("RPR_MATERIAL_NODE_OP_COMBINE"); }
		else if (a == RPR_MATERIAL_NODE_OP_DOT3) { printTrace("RPR_MATERIAL_NODE_OP_DOT3"); }
		else if (a == RPR_MATERIAL_NODE_OP_CROSS3) { printTrace("RPR_MATERIAL_NODE_OP_CROSS3"); }
		else if (a == RPR_MATERIAL_NODE_OP_LENGTH3) { printTrace("RPR_MATERIAL_NODE_OP_LENGTH3"); }
		else if (a == RPR_MATERIAL_NODE_OP_NORMALIZE3) { printTrace("RPR_MATERIAL_NODE_OP_NORMALIZE3"); }
		else if (a == RPR_MATERIAL_NODE_OP_POW) { printTrace("RPR_MATERIAL_NODE_OP_POW"); }
		else if (a == RPR_MATERIAL_NODE_OP_ACOS) { printTrace("RPR_MATERIAL_NODE_OP_ACOS"); }
		else if (a == RPR_MATERIAL_NODE_OP_ASIN) { printTrace("RPR_MATERIAL_NODE_OP_ASIN"); }
		else if (a == RPR_MATERIAL_NODE_OP_ATAN) { printTrace("RPR_MATERIAL_NODE_OP_ATAN"); }
		else if (a == RPR_MATERIAL_NODE_OP_AVERAGE_XYZ) { printTrace("RPR_MATERIAL_NODE_OP_AVERAGE_XYZ"); }
		else if (a == RPR_MATERIAL_NODE_OP_AVERAGE) { printTrace("RPR_MATERIAL_NODE_OP_AVERAGE"); }
		else if (a == RPR_MATERIAL_NODE_OP_MIN) { printTrace("RPR_MATERIAL_NODE_OP_MIN"); }
		else if (a == RPR_MATERIAL_NODE_OP_MAX) { printTrace("RPR_MATERIAL_NODE_OP_MAX"); }
		else if (a == RPR_MATERIAL_NODE_OP_FLOOR) { printTrace("RPR_MATERIAL_NODE_OP_FLOOR"); }
		else if (a == RPR_MATERIAL_NODE_OP_MOD) { printTrace("RPR_MATERIAL_NODE_OP_MOD"); }
		else if (a == RPR_MATERIAL_NODE_OP_ABS) { printTrace("RPR_MATERIAL_NODE_OP_ABS"); }
		else if (a == RPR_MATERIAL_NODE_OP_SHUFFLE_YZWX) { printTrace("RPR_MATERIAL_NODE_OP_SHUFFLE_YZWX"); }
		else if (a == RPR_MATERIAL_NODE_OP_SHUFFLE_ZWXY) { printTrace("RPR_MATERIAL_NODE_OP_SHUFFLE_ZWXY"); }
		else if (a == RPR_MATERIAL_NODE_OP_SHUFFLE_WXYZ) { printTrace("RPR_MATERIAL_NODE_OP_SHUFFLE_WXYZ"); }
		else if (a == RPR_MATERIAL_NODE_OP_MAT_MUL) { printTrace("RPR_MATERIAL_NODE_OP_MAT_MUL"); }
		else if (a == RPR_MATERIAL_NODE_OP_SELECT_W) { printTrace("RPR_MATERIAL_NODE_OP_SELECT_W"); }
		else if (a == RPR_MATERIAL_NODE_OP_DOT4) { printTrace("RPR_MATERIAL_NODE_OP_DOT4"); }
		else { printTrace("?%d?", a); }
	}
}


void Logger::TraceArg__rpr_composite_type(rpr_composite_type a)
{
	if (traceActivated)
	{
			 if (a == RPR_COMPOSITE_ARITHMETIC) { printTrace("RPR_COMPOSITE_ARITHMETIC"); }
		else if (a == RPR_COMPOSITE_LERP_VALUE) { printTrace("RPR_COMPOSITE_LERP_VALUE"); }
		else if (a == RPR_COMPOSITE_INVERSE) { printTrace("RPR_COMPOSITE_INVERSE"); }
		else if (a == RPR_COMPOSITE_NORMALIZE) { printTrace("RPR_COMPOSITE_NORMALIZE"); }
		else if (a == RPR_COMPOSITE_GAMMA_CORRECTION) { printTrace("RPR_COMPOSITE_GAMMA_CORRECTION"); }
		else if (a == RPR_COMPOSITE_EXPOSURE) { printTrace("RPR_COMPOSITE_EXPOSURE"); }
		else if (a == RPR_COMPOSITE_CONTRAST) { printTrace("RPR_COMPOSITE_CONTRAST"); }
		else if (a == RPR_COMPOSITE_SIDE_BY_SIDE) { printTrace("RPR_COMPOSITE_SIDE_BY_SIDE"); }
		else if (a == RPR_COMPOSITE_TONEMAP_ACES) { printTrace("RPR_COMPOSITE_TONEMAP_ACES"); }
		else if (a == RPR_COMPOSITE_TONEMAP_REINHARD) { printTrace("RPR_COMPOSITE_TONEMAP_REINHARD"); }
		else if (a == RPR_COMPOSITE_TONEMAP_LINEAR) { printTrace("RPR_COMPOSITE_TONEMAP_LINEAR"); }
		else if (a == RPR_COMPOSITE_FRAMEBUFFER) { printTrace("RPR_COMPOSITE_FRAMEBUFFER"); }
		else if (a == RPR_COMPOSITE_CONSTANT) { printTrace("RPR_COMPOSITE_CONSTANT"); }
		else { printTrace("?%d?", a); }
	}
}


void Logger::TraceArg__rpr_material_node_type(rpr_material_node_type a)
{
	if (traceActivated)
	{
		if (a == RPR_MATERIAL_NODE_DIFFUSE) { printTrace("RPR_MATERIAL_NODE_DIFFUSE"); }
		else if (a == RPR_MATERIAL_NODE_MICROFACET) { printTrace("RPR_MATERIAL_NODE_MICROFACET"); }
		else if (a == RPR_MATERIAL_NODE_REFLECTION) { printTrace("RPR_MATERIAL_NODE_REFLECTION"); }
		else if (a == RPR_MATERIAL_NODE_REFRACTION) { printTrace("RPR_MATERIAL_NODE_REFRACTION"); }
		else if (a == RPR_MATERIAL_NODE_MICROFACET_REFRACTION) { printTrace("RPR_MATERIAL_NODE_MICROFACET_REFRACTION"); }
		else if (a == RPR_MATERIAL_NODE_TRANSPARENT) { printTrace("RPR_MATERIAL_NODE_TRANSPARENT"); }
		else if (a == RPR_MATERIAL_NODE_EMISSIVE) { printTrace("RPR_MATERIAL_NODE_EMISSIVE"); }
		else if (a == RPR_MATERIAL_NODE_WARD) { printTrace("RPR_MATERIAL_NODE_WARD"); }
		else if (a == RPR_MATERIAL_NODE_ADD) { printTrace("RPR_MATERIAL_NODE_ADD"); }
		else if (a == RPR_MATERIAL_NODE_BLEND) { printTrace("RPR_MATERIAL_NODE_BLEND"); }
		else if (a == RPR_MATERIAL_NODE_ARITHMETIC) { printTrace("RPR_MATERIAL_NODE_ARITHMETIC"); }
		else if (a == RPR_MATERIAL_NODE_FRESNEL) { printTrace("RPR_MATERIAL_NODE_FRESNEL"); }
		else if (a == RPR_MATERIAL_NODE_NORMAL_MAP) { printTrace("RPR_MATERIAL_NODE_NORMAL_MAP"); }
		else if (a == RPR_MATERIAL_NODE_IMAGE_TEXTURE) { printTrace("RPR_MATERIAL_NODE_IMAGE_TEXTURE"); }
		else if (a == RPR_MATERIAL_NODE_NOISE2D_TEXTURE) { printTrace("RPR_MATERIAL_NODE_NOISE2D_TEXTURE"); }
		else if (a == RPR_MATERIAL_NODE_DOT_TEXTURE) { printTrace("RPR_MATERIAL_NODE_DOT_TEXTURE"); }
		else if (a == RPR_MATERIAL_NODE_GRADIENT_TEXTURE) { printTrace("RPR_MATERIAL_NODE_GRADIENT_TEXTURE"); }
		else if (a == RPR_MATERIAL_NODE_CHECKER_TEXTURE) { printTrace("RPR_MATERIAL_NODE_CHECKER_TEXTURE"); }
		else if (a == RPR_MATERIAL_NODE_CONSTANT_TEXTURE) { printTrace("RPR_MATERIAL_NODE_CONSTANT_TEXTURE"); }
		else if (a == RPR_MATERIAL_NODE_INPUT_LOOKUP) { printTrace("RPR_MATERIAL_NODE_INPUT_LOOKUP"); }
		else if (a == RPR_MATERIAL_NODE_STANDARD) { printTrace("RPR_MATERIAL_NODE_STANDARD"); }
		else if (a == RPR_MATERIAL_NODE_BLEND_VALUE) { printTrace("RPR_MATERIAL_NODE_BLEND_VALUE"); }
		else if (a == RPR_MATERIAL_NODE_PASSTHROUGH) { printTrace("RPR_MATERIAL_NODE_PASSTHROUGH"); }
		else if (a == RPR_MATERIAL_NODE_ORENNAYAR) { printTrace("RPR_MATERIAL_NODE_ORENNAYAR"); }
		else if (a == RPR_MATERIAL_NODE_FRESNEL_SCHLICK) { printTrace("RPR_MATERIAL_NODE_FRESNEL_SCHLICK"); }
		else if (a == RPR_MATERIAL_NODE_DIFFUSE_REFRACTION) { printTrace("RPR_MATERIAL_NODE_DIFFUSE_REFRACTION"); }
		else if (a == RPR_MATERIAL_NODE_BUMP_MAP) { printTrace("RPR_MATERIAL_NODE_BUMP_MAP"); }
		else if (a == RPR_MATERIAL_NODE_AO_MAP) { printTrace("RPR_MATERIAL_NODE_AO_MAP"); }
		else if (a == RPR_MATERIAL_NODE_VOLUME) { printTrace("RPR_MATERIAL_NODE_VOLUME"); }
		else if (a == RPR_MATERIAL_NODE_MICROFACET_ANISOTROPIC_REFLECTION) { printTrace("RPR_MATERIAL_NODE_MICROFACET_ANISOTROPIC_REFLECTION"); }
		else if (a == RPR_MATERIAL_NODE_MICROFACET_ANISOTROPIC_REFRACTION) { printTrace("RPR_MATERIAL_NODE_MICROFACET_ANISOTROPIC_REFRACTION"); }
		else if (a == RPR_MATERIAL_NODE_TWOSIDED) { printTrace("RPR_MATERIAL_NODE_TWOSIDED"); }
		else if (a == RPR_MATERIAL_NODE_UV_PROCEDURAL) { printTrace("RPR_MATERIAL_NODE_UV_PROCEDURAL"); }
		else if (a == RPR_MATERIAL_NODE_UV_TRIPLANAR) { printTrace("RPR_MATERIAL_NODE_UV_TRIPLANAR"); }
		else if (a == RPR_MATERIAL_NODE_BUFFER_SAMPLER) { printTrace("RPR_MATERIAL_NODE_BUFFER_SAMPLER"); }
		else if (a == RPR_MATERIAL_NODE_PHONG) { printTrace("RPR_MATERIAL_NODE_PHONG"); }
		else if (a == RPR_MATERIAL_NODE_MICROFACET_BECKMANN) { printTrace("RPR_MATERIAL_NODE_MICROFACET_BECKMANN"); }
		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_aov(rpr_aov a)
{
	if (traceActivated)
	{
			 if (a == RPR_AOV_COLOR)				{ printTrace("RPR_AOV_COLOR"); }
		else if (a == RPR_AOV_OPACITY)			{ printTrace("RPR_AOV_OPACITY"); }
		else if (a == RPR_AOV_WORLD_COORDINATE)	{ printTrace("RPR_AOV_WORLD_COORDINATE"); }
		else if (a == RPR_AOV_UV)				{ printTrace("RPR_AOV_UV"); }
		else if (a == RPR_AOV_MATERIAL_IDX)		{ printTrace("RPR_AOV_MATERIAL_IDX"); }
		else if (a == RPR_AOV_GEOMETRIC_NORMAL)	{ printTrace("RPR_AOV_GEOMETRIC_NORMAL"); }
		else if (a == RPR_AOV_SHADING_NORMAL)	{ printTrace("RPR_AOV_SHADING_NORMAL"); }
		else if (a == RPR_AOV_DEPTH)				{ printTrace("RPR_AOV_DEPTH"); }
		else if (a == RPR_AOV_OBJECT_ID)			{ printTrace("RPR_AOV_OBJECT_ID"); }
		else if (a == RPR_AOV_OBJECT_GROUP_ID)			{ printTrace("RPR_AOV_OBJECT_GROUP_ID"); }
		else if (a == RPR_AOV_SHADOW_CATCHER)			{ printTrace("RPR_AOV_SHADOW_CATCHER"); }
		else if (a == RPR_AOV_BACKGROUND)			{ printTrace("RPR_AOV_BACKGROUND"); }
		else if (a == RPR_AOV_EMISSION)			{ printTrace("RPR_AOV_EMISSION"); }
		else if (a == RPR_AOV_VELOCITY)				{ printTrace("RPR_AOV_VELOCITY"); }

		else if (a == RPR_AOV_LIGHT_GROUP0)				{ printTrace("RPR_AOV_LIGHT_GROUP0"); }
		else if (a == RPR_AOV_LIGHT_GROUP1)				{ printTrace("RPR_AOV_LIGHT_GROUP1"); }
		else if (a == RPR_AOV_LIGHT_GROUP2)				{ printTrace("RPR_AOV_LIGHT_GROUP2"); }
		else if (a == RPR_AOV_LIGHT_GROUP3)				{ printTrace("RPR_AOV_LIGHT_GROUP3"); }

		else if (a == RPR_AOV_DIRECT_ILLUMINATION)	{ printTrace("RPR_AOV_DIRECT_ILLUMINATION"); }
		else if (a == RPR_AOV_INDIRECT_ILLUMINATION){ printTrace("RPR_AOV_INDIRECT_ILLUMINATION"); }

		else if (a == RPR_AOV_AO)					{ printTrace("RPR_AOV_AO"); }
		else if (a == RPR_AOV_DIRECT_DIFFUSE)		{ printTrace("RPR_AOV_DIRECT_DIFFUSE"); }
		else if (a == RPR_AOV_DIRECT_REFLECT)		{ printTrace("RPR_AOV_DIRECT_REFLECT"); }
		else if (a == RPR_AOV_INDIRECT_DIFFUSE)		{ printTrace("RPR_AOV_INDIRECT_DIFFUSE"); }
		else if (a == RPR_AOV_INDIRECT_REFLECT)		{ printTrace("RPR_AOV_INDIRECT_REFLECT"); }
		else if (a == RPR_AOV_REFRACT)				{ printTrace("RPR_AOV_REFRACT"); }
		else if (a == RPR_AOV_VOLUME)				{ printTrace("RPR_AOV_VOLUME"); }

		else { printTrace("?%d?", a); }
	}
}

void Logger::TraceArg__rpr_material_node_input(rpr_material_node_input a)
{
	if (traceActivated)
	{
			 if (a == RPR_MATERIAL_INPUT_COLOR) { printTrace("RPR_MATERIAL_INPUT_COLOR"); }
		else if (a == RPR_MATERIAL_INPUT_COLOR0) { printTrace("RPR_MATERIAL_INPUT_COLOR0"); }
		else if (a == RPR_MATERIAL_INPUT_COLOR1) { printTrace("RPR_MATERIAL_INPUT_COLOR1"); }
		else if (a == RPR_MATERIAL_INPUT_COLOR2) { printTrace("RPR_MATERIAL_INPUT_COLOR2"); }
		else if (a == RPR_MATERIAL_INPUT_COLOR3) { printTrace("RPR_MATERIAL_INPUT_COLOR3"); }
		else if (a == RPR_MATERIAL_INPUT_NORMAL) { printTrace("RPR_MATERIAL_INPUT_NORMAL"); }
		else if (a == RPR_MATERIAL_INPUT_UV) { printTrace("RPR_MATERIAL_INPUT_UV"); }
		else if (a == RPR_MATERIAL_INPUT_DATA) { printTrace("RPR_MATERIAL_INPUT_DATA"); }
		else if (a == RPR_MATERIAL_INPUT_ROUGHNESS) { printTrace("RPR_MATERIAL_INPUT_ROUGHNESS"); }
		else if (a == RPR_MATERIAL_INPUT_IOR) { printTrace("RPR_MATERIAL_INPUT_IOR"); }
		else if (a == RPR_MATERIAL_INPUT_ROUGHNESS_X) { printTrace("RPR_MATERIAL_INPUT_ROUGHNESS_X"); }
		else if (a == RPR_MATERIAL_INPUT_ROUGHNESS_Y) { printTrace("RPR_MATERIAL_INPUT_ROUGHNESS_Y"); }
		else if (a == RPR_MATERIAL_INPUT_ROTATION) { printTrace("RPR_MATERIAL_INPUT_ROTATION"); }
		else if (a == RPR_MATERIAL_INPUT_WEIGHT) { printTrace("RPR_MATERIAL_INPUT_WEIGHT"); }
		else if (a == RPR_MATERIAL_INPUT_OP) { printTrace("RPR_MATERIAL_INPUT_OP"); }
		else if (a == RPR_MATERIAL_INPUT_INVEC) { printTrace("RPR_MATERIAL_INPUT_INVEC"); }
		else if (a == RPR_MATERIAL_INPUT_UV_SCALE) { printTrace("RPR_MATERIAL_INPUT_UV_SCALE"); }
		else if (a == RPR_MATERIAL_INPUT_VALUE) { printTrace("RPR_MATERIAL_INPUT_VALUE"); }
		else if (a == RPR_MATERIAL_INPUT_REFLECTANCE) { printTrace("RPR_MATERIAL_INPUT_REFLECTANCE"); }
		else if (a == RPR_MATERIAL_INPUT_SCALE) { printTrace("RPR_MATERIAL_INPUT_SCALE"); }
		else if (a == RPR_MATERIAL_INPUT_ANISOTROPIC) { printTrace("RPR_MATERIAL_INPUT_ANISOTROPIC"); }
		else if (a == RPR_MATERIAL_INPUT_FRONTFACE) { printTrace("RPR_MATERIAL_INPUT_FRONTFACE"); }
		else if (a == RPR_MATERIAL_INPUT_BACKFACE) { printTrace("RPR_MATERIAL_INPUT_BACKFACE"); }
		else if (a == RPR_MATERIAL_INPUT_ORIGIN) { printTrace("RPR_MATERIAL_INPUT_ORIGIN"); }
		else if (a == RPR_MATERIAL_INPUT_ZAXIS) { printTrace("RPR_MATERIAL_INPUT_ZAXIS"); }
		else if (a == RPR_MATERIAL_INPUT_XAXIS) { printTrace("RPR_MATERIAL_INPUT_XAXIS"); }
		else if (a == RPR_MATERIAL_INPUT_THRESHOLD) { printTrace("RPR_MATERIAL_INPUT_THRESHOLD"); }
		else if (a == RPR_MATERIAL_INPUT_OFFSET) { printTrace("RPR_MATERIAL_INPUT_OFFSET"); }
		else if (a == RPR_MATERIAL_INPUT_UV_TYPE) { printTrace("RPR_MATERIAL_INPUT_UV_TYPE"); }
		else if (a == RPR_MATERIAL_INPUT_RADIUS) { printTrace("RPR_MATERIAL_INPUT_RADIUS"); }
		else if (a == RPR_MATERIAL_INPUT_SIDE) { printTrace("RPR_MATERIAL_INPUT_SIDE"); }
		
		else { printTrace("?%d?", a); }
	}
}

void Logger::Trace__FunctionClose()
{
	if (traceActivated)
	{
		printTrace(");  RPRTRACE_CHECK\r\n");
		fflush(flog);
		flog_data.flush();


		m_printfFileCount++;
		if (m_printfFileCount >= m_nbInstructionsPerFile)
		{
			m_printfFileCount = 0;
			SetupNextRecordPage();
		}

	}
}

void Logger::Trace__FunctionOpen(const char* functionName)
{
	if (traceActivated)
	{
		printTrace("%s(", functionName);
	}
}

void Logger::Trace__FunctionFailed(void* frNode, const char* functionName,rpr_int errorCode)
{
	if (traceActivated)
	{

		printTrace("// WARNING : %s FAILED with error : ", functionName);

			 if ( errorCode == RPR_SUCCESS ) { printTrace("RPR_SUCCESS"); }
		else if ( errorCode == RPR_ERROR_COMPUTE_API_NOT_SUPPORTED ) { printTrace("RPR_ERROR_COMPUTE_API_NOT_SUPPORTED"); }
		else if ( errorCode == RPR_ERROR_OUT_OF_SYSTEM_MEMORY ) { printTrace("RPR_ERROR_OUT_OF_SYSTEM_MEMORY"); }
		else if ( errorCode == RPR_ERROR_OUT_OF_VIDEO_MEMORY ) { printTrace("RPR_ERROR_OUT_OF_VIDEO_MEMORY"); }
		else if ( errorCode == RPR_ERROR_INVALID_LIGHTPATH_EXPR ) { printTrace("RPR_ERROR_INVALID_LIGHTPATH_EXPR"); }
		else if ( errorCode == RPR_ERROR_INVALID_IMAGE ) { printTrace("RPR_ERROR_INVALID_IMAGE"); }
		else if ( errorCode == RPR_ERROR_INVALID_AA_METHOD ) { printTrace("RPR_ERROR_INVALID_AA_METHOD"); }
		else if ( errorCode == RPR_ERROR_UNSUPPORTED_IMAGE_FORMAT ) { printTrace("RPR_ERROR_UNSUPPORTED_IMAGE_FORMAT"); }
		else if ( errorCode == RPR_ERROR_INVALID_GL_TEXTURE ) { printTrace("RPR_ERROR_INVALID_GL_TEXTURE"); }
		else if ( errorCode == RPR_ERROR_INVALID_CL_IMAGE ) { printTrace("RPR_ERROR_INVALID_CL_IMAGE"); }
		else if ( errorCode == RPR_ERROR_INVALID_OBJECT ) { printTrace("RPR_ERROR_INVALID_OBJECT"); }
		else if ( errorCode == RPR_ERROR_INVALID_PARAMETER ) { printTrace("RPR_ERROR_INVALID_PARAMETER"); }
		else if ( errorCode == RPR_ERROR_INVALID_TAG ) { printTrace("RPR_ERROR_INVALID_TAG"); }
		else if ( errorCode == RPR_ERROR_INVALID_LIGHT ) { printTrace("RPR_ERROR_INVALID_LIGHT"); }
		else if ( errorCode == RPR_ERROR_INVALID_CONTEXT ) { printTrace("RPR_ERROR_INVALID_CONTEXT"); }
		else if ( errorCode == RPR_ERROR_UNIMPLEMENTED ) { printTrace("RPR_ERROR_UNIMPLEMENTED"); }
		else if ( errorCode == RPR_ERROR_INVALID_API_VERSION ) { printTrace("RPR_ERROR_INVALID_API_VERSION"); }
		else if ( errorCode == RPR_ERROR_INTERNAL_ERROR ) { printTrace("RPR_ERROR_INTERNAL_ERROR"); }
		else if ( errorCode == RPR_ERROR_IO_ERROR ) { printTrace("RPR_ERROR_IO_ERROR"); }
		else if ( errorCode == RPR_ERROR_UNSUPPORTED_SHADER_PARAMETER_TYPE ) { printTrace("RPR_ERROR_UNSUPPORTED_SHADER_PARAMETER_TYPE"); }
		else if ( errorCode == RPR_ERROR_MATERIAL_STACK_OVERFLOW ) { printTrace("RPR_ERROR_MATERIAL_STACK_OVERFLOW"); }
		else if ( errorCode == RPR_ERROR_INVALID_PARAMETER_TYPE ) { printTrace("RPR_ERROR_INVALID_PARAMETER_TYPE"); }
		else if ( errorCode == RPR_ERROR_UNSUPPORTED ) { printTrace("RPR_ERROR_UNSUPPORTED"); }
		else  { printTrace("%d",errorCode); }

		//if possible, add  CONTEXT_LAST_ERROR_MESSAGE,  help to debug
		if ( frNode )
		{
			FrNode* nodefr = (FrNode*)frNode;
			FrNode* node_Context = nullptr;

			if ( nodefr->GetType() == NodeTypes::Context ) // if it's a context, we use it directly
			{
				node_Context = nodefr;
			}
			else // if it's not a context get the context from the node
			{
				node_Context = nodefr->GetProperty<FrNode*>(FR_NODE_CONTEXT);
			}

			if ( node_Context )
			{
				std::string cachePath = node_Context->GetProperty<std::string>(RPR_CONTEXT_LAST_ERROR_MESSAGE);
				printTrace("\r\n// CONTEXT_LAST_ERROR_MESSAGE = "); 
				TraceArg__rpr_char_P(cachePath.c_str());
			}
		}


		printTrace("\r\n");

		Trace__FlushAllFiles();
	}
}


void Logger::Trace__NewFrObjectCreated(TRACE_ADDRESS_TYPE type, const void* address)
{
	if (traceActivated)
	{
		m_trace_addressToType[address] = type;
	}
}

void Logger::Trace__rprContextSetAOV(rpr_context context, rpr_aov aov, rpr_framebuffer frame_buffer)
{
	if (traceActivated)
	{
		if (aov == RPR_AOV_COLOR )
		{
			m_mapContextToFramebuffer[context] = frame_buffer;
		}
	}
}


void Logger::Trace__CommentPossibilityExport_contex(rpr_context context)
{
	if (traceActivated)
	{
		printTrace("//rprFrameBufferSaveToFile(");
		if (m_mapContextToFramebuffer.find(context) != m_mapContextToFramebuffer.end())
		{
			TraceArg__rpr_framebuffer(m_mapContextToFramebuffer[context]);
		}
		else
		{
			printTrace("unknownFrameBuffer");
		}
		printTrace(", \"img_%05d.png\"); // <-- uncomment if you want export image\r\n\r\n", m_trace_countExportImage);
		m_trace_countExportImage++;
	}
}

void Logger::Trace__CommentPossibilityExport_framebuf(rpr_framebuffer frame_buffer)
{
	if (traceActivated)
	{
		printTrace("//rprFrameBufferSaveToFile(");
		TraceArg__rpr_framebuffer(frame_buffer);
		printTrace(", \"img_%05d.png\"); // <-- uncomment if you want export image\r\n\r\n", m_trace_countExportImage);
		m_trace_countExportImage++;
	}
}

void Logger::Trace__DeclareFRobject(const char* declar, const char* prefix, void* obj)
{
	if (traceActivated)
	{
		std::string objectFullName;

		if ( obj == nullptr && strcmp(prefix,"tahoePluginID_0x") == 0 )
		{
			// special case :
			// because of issues with linux, tahoePluginID 0  is hardcoded  tahoePluginID_0x0000
			// ( instead of  tahoePluginID_0x0000000000000000 )
			objectFullName = std::string("tahoePluginID_0x0000"); 
		}
		else
		{
			std::stringstream stream;
			stream << prefix;
			stream << std::hex << obj;
			objectFullName = std::string(stream.str());
		}

		auto found = find(m_trace_listDeclaredFrObject.begin(), m_trace_listDeclaredFrObject.end(), objectFullName);
		if (found != m_trace_listDeclaredFrObject.end() )
		{
			printTrace("%s = NULL;", objectFullName.c_str());
		}
		else
		{
			printVariable("%s %s = NULL;\r\n", declar, objectFullName.c_str());
			m_trace_listDeclaredFrObject.push_back(objectFullName);
		}

	}
}

void Logger::SetupNextRecordPage()
{
	if (traceActivated)
	{

		m_recordFileIndex++;

		
		if (flog) 
		{ 
			printTrace("\treturn RPR_SUCCESS;\r\n");
			printTrace("}\r\n");
			fclose(flog); 
			flog = NULL; 
		}

		printPlaylist("\\\r\nRPRTRACINGMACRO__PLAYLIST_BEG play%d(); RPRTRACINGMACRO__PLAYLIST_END ", m_recordFileIndex);

		std::string fullpath_cpp = m_folderOfTraceFile + std::string("rprTrace_play") + std::to_string(m_recordFileIndex) + std::string(".cpp");

		flog = fopen(fullpath_cpp.c_str(), "wb");

		if (flog == NULL)
		{
			//if we can't open the trace file (maybe because read-only, or because not permission...etc... )
			//we disable tracing
			if (flog) { fclose(flog); flog = NULL; }
			if (flog_variables) { fclose(flog_variables); flog_variables = NULL; }
			if (flog_playList) { fclose(flog_playList); flog_playList = NULL; }
			if (flog_playerClassCpp) { fclose(flog_playerClassCpp); flog_playerClassCpp = NULL; }
			if (flog_playerClassH) { fclose(flog_playerClassH); flog_playerClassH = NULL; }
			if (flog_data.is_open()) { flog_data.close(); }
			traceActivated = false;
			return;
		}


		printTrace("#include \"rprTrace_player.h\"\r\n");
		printTrace("int RprTracePlayer::play%d()\r\n", m_recordFileIndex);
		printTrace("{\r\n");

	}
}

void Logger::ImportFileInTraceFolders(const rpr_char* filePath, std::string& newFileNameLocal)
{
	if (traceActivated)
	{
		if ( !filePath )
		{
			return;

		}

		int64_t len = strlen(filePath);

		std::string extension = "";
		bool extension_correct = false;

		if ( len >= 3 )
		{

			std::string extension_reversed;
			
			for(int64_t i=len-1; i>=0; i--)
			{
				if ( filePath[i] == '.' )
				{
					extension_correct = true;
					break;
				}
				if ( filePath[i] == '/' || filePath[i] == '\\' )
				{
					break;
				}
				extension_reversed += filePath[i];
			}

			
			if ( extension_correct )
			{
				for(int i=0; i<extension_reversed.length(); i++)
				{
					extension += extension_reversed[extension_reversed.length()-1-i];
				}
			}


		}




		newFileNameLocal =  "rprTrace_dataFile_";
		newFileNameLocal += std::to_string(m_fileImportedCounter);
		
		if ( extension_correct )
		{
			newFileNameLocal += ".";
			newFileNameLocal += extension;
		}


		std::string newFile = m_folderOfTraceFile + newFileNameLocal;

		//copy file in the trace folder
		std::ifstream  src(filePath, std::ios::binary);
		std::ofstream  dst(newFile,   std::ios::binary);
		dst << src.rdbuf();


		m_fileImportedCounter++;

	}

}


