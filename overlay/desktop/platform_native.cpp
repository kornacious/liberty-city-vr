#include "platform_desktop.h"
#include <cstdio>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
namespace {
std::atomic<const char*> *checkpoint=nullptr;
LONG WINAPI crashReport(EXCEPTION_POINTERS *exception) {
    auto process=GetCurrentProcess();
    wchar_t executable[32768]{}; GetModuleFileNameW(nullptr,executable,32768);
    auto symbols=std::filesystem::path(executable).parent_path().wstring();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    std::fprintf(stderr,"PCVR crash: exception %08lx, checkpoint %s\n",exception->ExceptionRecord->ExceptionCode,checkpoint->load());
    if(SymInitializeW(process,symbols.c_str(),TRUE)) {
        CONTEXT context=*exception->ContextRecord;
        STACKFRAME64 frame{}; frame.AddrPC.Offset=context.Rip; frame.AddrStack.Offset=context.Rsp; frame.AddrFrame.Offset=context.Rbp;
        frame.AddrPC.Mode=frame.AddrStack.Mode=frame.AddrFrame.Mode=AddrModeFlat;
        for(int i=0;i<24;++i) {
            alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+1024]{};
            auto symbol=reinterpret_cast<SYMBOL_INFO*>(storage); symbol->SizeOfStruct=sizeof(SYMBOL_INFO); symbol->MaxNameLen=1024;
            DWORD64 displacement=0; IMAGEHLP_LINE64 line{}; line.SizeOfStruct=sizeof(line); DWORD lineDisplacement=0;
            if(SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol)) {
                if(SymGetLineFromAddr64(process,frame.AddrPC.Offset,&lineDisplacement,&line))
                    std::fprintf(stderr,"PCVR stack: %s (%s:%lu)\n",symbol->Name,line.FileName,line.LineNumber);
                else std::fprintf(stderr,"PCVR stack: %s\n",symbol->Name);
            }
            if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,GetCurrentThread(),&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr)) break;
        }
        SymCleanup(process);
    }
    std::fflush(stderr); return EXCEPTION_EXECUTE_HANDLER;
}
}
namespace platform { void installNativeDiagnostics(std::atomic<const char*> *value){checkpoint=value;SetUnhandledExceptionFilter(crashReport);} }
#else
#include <signal.h>
#include <unistd.h>
namespace {
std::atomic<const char*> *checkpoint=nullptr;
void crash(int signal){const char prefix[]="PCVR crash; last checkpoint: ";write(STDERR_FILENO,prefix,sizeof(prefix)-1);const char *text=checkpoint?checkpoint->load(std::memory_order_relaxed):"unknown";size_t length=0;while(text[length] && length<128)++length;write(STDERR_FILENO,text,length);write(STDERR_FILENO,"\n",1);raise(signal);}
}
namespace platform {void installNativeDiagnostics(std::atomic<const char*> *value){checkpoint=value;struct sigaction action{};action.sa_handler=crash;action.sa_flags=SA_RESETHAND;sigemptyset(&action.sa_mask);for(int signal:{SIGSEGV,SIGABRT,SIGILL,SIGFPE,SIGBUS})sigaction(signal,&action,nullptr);}}
#endif
