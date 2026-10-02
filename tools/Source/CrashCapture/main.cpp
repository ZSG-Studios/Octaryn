#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <string>

static void trace(HANDLE process,DWORD tid) {
  HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,tid);
  CONTEXT context{};context.ContextFlags=CONTEXT_ALL;
  if(!thread || !GetThreadContext(thread,&context)){if(thread)CloseHandle(thread);return;}
  STACKFRAME64 frame{};frame.AddrPC={context.Rip,0,AddrModeFlat};
  frame.AddrStack={context.Rsp,0,AddrModeFlat};frame.AddrFrame={context.Rbp,0,AddrModeFlat};
  for(unsigned index=0;index<64 && frame.AddrPC.Offset;++index) {
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
    auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=MAX_SYM_NAME;
    DWORD64 displacement{};const bool named=SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol)!=FALSE;
    IMAGEHLP_LINE64 line{};line.SizeOfStruct=sizeof(line);DWORD offset{};
    const bool located=SymGetLineFromAddr64(process,frame.AddrPC.Offset,&offset,&line)!=FALSE;
    std::printf("crash_frame index=%u address=0x%llx symbol=%s offset=%llu file=%s line=%lu\n",index,
      frame.AddrPC.Offset,named?symbol->Name:"unknown",displacement,located?line.FileName:"unknown",located?line.LineNumber:0ul);
    if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&context,nullptr,
        SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
  }
  std::fflush(stdout);CloseHandle(thread);
}
int wmain(int argc,wchar_t** argv) {
  if(argc<2)return 2;
  std::wstring command;
  for(int index=1;index<argc;++index) {
    std::wstring value=argv[index];if(value.find(L'"')!=value.npos || value.size()>4096)return 2;
    command+=(index>1?L" ":L"")+std::wstring(L"\"")+value+L"\"";
  }
  STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
  startup.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);startup.hStdError=GetStdHandle(STD_ERROR_HANDLE);
  startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION process{};
  if(!CreateProcessW(argv[1],command.data(),nullptr,nullptr,TRUE,DEBUG_ONLY_THIS_PROCESS|CREATE_NO_WINDOW,
      nullptr,nullptr,&startup,&process))return 2;
  SymSetOptions(SYMOPT_LOAD_LINES|SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS);
  SymInitialize(process.hProcess,nullptr,FALSE);DEBUG_EVENT event{};bool failed=false;
  for(;;) {
    if(!WaitForDebugEvent(&event,1000)) {
      if(GetLastError()==ERROR_SEM_TIMEOUT)continue;
      failed=true;break;
    }
    DWORD status=DBG_CONTINUE;
    if(event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT || event.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT) {
      HANDLE file=event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT?event.u.CreateProcessInfo.hFile:event.u.LoadDll.hFile;
      auto* base=event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT?event.u.CreateProcessInfo.lpBaseOfImage:event.u.LoadDll.lpBaseOfDll;
      wchar_t path[4096]{};
      if(file && GetFinalPathNameByHandleW(file,path,4096,0)) {
        const wchar_t* normalized=path;
        if(std::wstring(path).starts_with(L"\\\\?\\"))normalized+=4;
        const auto loaded=SymLoadModuleExW(process.hProcess,file,normalized,nullptr,reinterpret_cast<DWORD64>(base),0,nullptr,0);
        std::printf("crash_module base=%p symbols=0x%llx path=%ls\n",base,loaded,normalized);std::fflush(stdout);
      }
      if(file)CloseHandle(file);
    } else if(event.dwDebugEventCode==CREATE_THREAD_DEBUG_EVENT) {
      CloseHandle(event.u.CreateThread.hThread);
    } else if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT) {
      const auto& exception=event.u.Exception;
      if(exception.ExceptionRecord.ExceptionCode!=EXCEPTION_BREAKPOINT)status=DBG_EXCEPTION_NOT_HANDLED;
      if(!exception.dwFirstChance) {
        std::printf("crash_exception code=0x%08lx address=%p thread=%lu\n",exception.ExceptionRecord.ExceptionCode,
          exception.ExceptionRecord.ExceptionAddress,event.dwThreadId);
        trace(process.hProcess,event.dwThreadId);failed=true;
      }
    } else if(event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT) {
      failed|=event.u.ExitProcess.dwExitCode!=0;ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status);break;
    }
    ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status);
  }
  SymCleanup(process.hProcess);CloseHandle(process.hThread);CloseHandle(process.hProcess);return failed?1:0;
}
