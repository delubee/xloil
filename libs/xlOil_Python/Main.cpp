#include "PyHelpers.h"
#include "PyEvents.h"
#include "TypeConversion/BasicTypes.h"
#include "PySource.h"
#include "PyAddin.h"
#include "PyFunctionRegister.h"
#include "EventLoop.h"

#include <xlOil/Register.h>
#include <xlOil/ExcelObj.h>
#include <xlOil/Log.h>
#include <xloil/Plugin.h>
#include <xloil/StringUtils.h>
#include <xlOil/Events.h>
#include <xlOil/ExcelThread.h>
#include <xlOil/ExcelUI.h>
#include <xlOilHelpers/Environment.h>
#include <xlOilHelpers/Settings.h>
#include <xlOil/AppObjects.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/embed.h>
#include <functional>
#include <cstdlib>
#include <filesystem>
#define TOML_ABI_NAMESPACES 0
#include <toml++/toml.h>
#include <boost/preprocessor/stringize.hpp>
#include <signal.h>
#include <thread>
#include <CTPL/ctpl_stl.h>

namespace fs = std::filesystem;

using std::vector;
using std::wstring;
using std::string;
using std::function;
using std::shared_ptr;
using std::make_shared;

namespace py = pybind11;

// Some fun C signal handling to work-around the calling of abort when
// Py_Initialize fails. In Py >= 3.8, a two-step initialisation is 
// possible to avoid this problem.
namespace
{
  const auto DEFAULT_COM_BINDER = "win32com"s;

#if PY_MAJOR_VERSION <= 3 && PY_MINOR_VERSION < 8
  jmp_buf longjumpBuffer;

  void signalHandler(int signum)
  {
    if (signum == SIGABRT)
    {
      // Reset signal handler
      signal(signum, SIG_DFL);
      // Never return - jump back to saved call site
      longjmp(longjumpBuffer, 1);
    }
  }
#else
  void checkReturnStatus(const PyStatus& status)
  {
    if (PyStatus_Exception(status))
      XLO_THROW("Python init failed: {}", status.err_msg ? status.err_msg : "Unknown error");
  }
#endif
}

namespace xloil
{
  namespace Python
  {
    namespace
    {
      shared_ptr<PyAddin> theCoreAddinContext;
      shared_ptr<const void> theWorkbookOpenHandler;


      py::list pythonSysPath()
      {
        return PyBorrow<py::list>(PySys_GetObject("path"));
      }

      string getPythonEnvDetails()
      {
        const string searchPath = py::str(pythonSysPath());
        const string prefix = py::str(PyBorrow<>(PySys_GetObject("prefix")));
        const string basePrefix = py::str(PyBorrow<>(PySys_GetObject("base_prefix")));
        return formatStr("sys.prefix=%s\nsys.base_prefix=%s\nsys.path=%s",
          prefix.c_str(), basePrefix.c_str(), searchPath.c_str());
      }

      void appendSysPath(const wstring& x)
      {
        auto sysPath = pythonSysPath();
        sysPath.append(py::wstr(x));
      }

      auto addinDir(AddinContext& ctx)
      {
        return fs::path(ctx.pathName()).remove_filename().wstring();
      }

      void startInterpreter(const std::wstring& setSysPath)
      {
        (void)setSysPath;

        if (Py_IsInitialized())
          XLO_THROW(L"Python already initialised: Only one python plugin can be used");

        PyImport_AppendInittab(theInjectedModuleName, &buildInjectedModule);

        // The SetSysPath option can be useful when distributing an addin along
        // with all required python libs.


        XLO_DEBUG("Python interpreter starting");

        // Determine the path to the python.exe associated with our interpreter
        // Some code assumes `sys.executable` points to a valid python.exe, in particular
        // site.py uses this to check for the presence of a virtual environment. In an 
        // embedded interpreter, this has to be done explicitly - see below.
        auto executablePath = fs::path(getEnvironmentVar(L"PYTHONEXECUTABLE"));

        // 
        // In Py <= 3.7 some fun C signal handling is required to work-around the 
        // interpreter calling abort when Py_Initialize fails which is not friendly 
        // in an embedded setting.
        // In Py >= 3.8, a two-step initialisation is possible to avoid this problem.
        //
#if PY_VERSION_HEX < 0x03080000
        if (!setSysPath.empty())
          Py_SetPath(setSysPath.c_str());

        static auto programName = fs::path(getEnvironmentVar(L"PYTHONEXECUTABLE")).wstring();
        // Python API docs say that setting Py_SetProgramName should set sys.executable.  
        // However it's bugged in Windows, see https://bugs.python.org/issue34725,  
        // fortunately an undocumented call _Py_SetProgramFullPath in Py 3.7 can fix this. 
        // In Py 3.8+, the new interpreter config handles this more elegantly.
        Py_SetProgramName(const_cast<wchar_t*>(programName.c_str()));
#if PY_VERSION_HEX < 0x03070000
#else
        _Py_SetProgramFullPath(programName.c_str());
#endif
        {
          // The first time setjmp executes the return value will be zero. 
          // If abort is called, execution will resume with setjmp returning
          // the value 1
          if (setjmp(longjumpBuffer) == 0)
          {
            signal(SIGABRT, &signalHandler);
            Py_InitializeEx(0);
            signal(SIGABRT, SIG_DFL);   // Reset signal handler
          }
          else
          {
            XLO_THROW("Python initialisation called abort. Check 'encodings' "
              "package is available on configured python search paths.");
          }
        }

#if PY_VERSION_HEX < 0x03070000
        // _Py_SetProgramFullPath doesn't exist in Py 3.6, so we try this hacky 
        // override. The site module initialisation uses sys.executable so we re-run it
        py::setattr(
          py::module::import("sys"), 
          "executable", 
          py::str(executablePath.string()));
        py::module::import("site").attr("main")();
#endif

#else
        {
          PyConfig config;
          PyConfig_InitPythonConfig(&config);

          std::shared_ptr<PyConfig> cleanup(
            &config, [](PyConfig * p){ PyConfig_Clear(p); });

          auto pythonPath = getEnvironmentVar(L"PYTHONPATH");

          // Set `sys.executable` - see comment above
          checkReturnStatus(
            PyConfig_SetString(&config, &config.program_name, executablePath.c_str()));
          
          // Despite setting PYTHONPATH before the python DLL is loaded, somehow it 
          // sometimes fails to find the value we set; so we explicitly pull it from
          // the env table and force it through here. I suspect this has to do with
          // different env tables in different versions of the CRT but I have no idea
          // how to fix this any other way.
          checkReturnStatus(
            PyConfig_SetString(&config, &config.pythonpath_env, pythonPath.c_str()));

          config.use_environment = 1;
          config.parse_argv = 0;        // No point parsing cmd line args

          checkReturnStatus(PyConfig_Read(&config));

          checkReturnStatus(
            PyConfig_SetString(&config, &config.executable, executablePath.c_str()));

          checkReturnStatus(Py_InitializeFromConfig(&config));
        }
#endif

#if PY_VERSION_HEX < 0x03070000
        PyEval_InitThreads();
#endif

        // Release the GIL when we hand back control
        PyEval_SaveThread();
      }

      void exit()
      {
        try
        {
          Event_PyBye().fire();
        }
        catch (const std::exception& e)
        {
          XLO_ERROR("PyBye: {0}", e.what());
        }
        try
        {
          PyGILState_Ensure();
          py::finalize_interpreter();
        }
        catch (...)
        {
        }
      }
    }

    const std::shared_ptr<PyAddin>& theCoreAddin()
    {
      return theCoreAddinContext;
    }

    extern "C" __declspec(dllexport) int xloil_python_init(
      AddinContext* context, const PluginContext& plugin)
    {
      try
      {
        switch (plugin.action)
        {
        case PluginContext::Load:
        {
          assert(context);

          // On Load, we initialise the Python interpreter and import our 
          // pybind11 module
          linkPluginToCoreLogger(context, plugin);

          const auto setSysPath = utf8ToUtf16(plugin.settings["SetSysPath"].value_or(""));

          // Check the workbook module setting for loading local modules of the form 'Book1.py'.
          const auto workbookModulePattern = utf8ToUtf16(
            plugin.settings["WorkbookModule"].value_or(""));

          // Warn if people are using a pre-0.16 ini file with PYTHONHOME but no
          // PYTHONEXECUTABLE. If they have both, they may be using PYTHONHOME for 
          // so other purpose, so we assume they know what they are doing
          {
            auto environmentVars = Settings::environmentVariables(
                toml::view_node(&plugin.settings));
            auto flag = 0;
            for (auto& [key, val] : environmentVars)
            {
              if (key == L"PYTHONHOME") ++flag;
              if (key == L"PYTHONEXECUTABLE") --flag;
            }
            if (flag > 0)
              XLO_WARN("Setting PYTHONHOME is no longer advised. Instead set PYTHONEXECUTABLE"
                " to point to your distribution's python.exe"
              );
          }

          // Starts the python interpreter with our embedded module available
          startInterpreter(setSysPath);

          // startInterpreter releases the gil on completion
          py::gil_scoped_acquire getGil;

          appendSysPath(addinDir(*context));

          // Since xloil imports importlib, it cannot be the first module imported by python
          // otherwise some bootstrap processes have not completed and xloil gets an incomplete
          // importlib module.
          // See https://stackoverflow.com/questions/39660934/
          py::module::import("importlib.util");

          // https://bugs.python.org/issue37416
          py::module::import("threading");

          if (spdlog::default_logger()->level() <= spdlog::level::info)
            XLO_INFO("Python started with environment: {}", getPythonEnvDetails());

          importDatetime();

          XLO_DEBUG("Python importing xloil_core");
          py::module::import(theInjectedModuleName);

          // On Load, the core context is created with a new thread and event
          // loop. We must release gil before creating a PyAddin
          {
            py::gil_scoped_release releaseGil;
            auto pyContext = make_shared<PyAddin>(
              *context,
              true,
              plugin.settings["ComLib"].value_or(DEFAULT_COM_BINDER),
              workbookModulePattern,
              plugin.settings["UseLoaderThread"].value_or(true));
            theCoreAddinContext = pyContext;
          }

          XLO_DEBUG("Python importing xloil");
          py::module::import("xloil");

          theCoreAddinContext->importModule(py::cast("xloil.excelfuncs"));

          // Check for a debugger specified in the settings
          const auto debugger = plugin.settings["Debugger"].value_or(string(""));
          if (!debugger.empty())
            py::module::import("xloil.debug").attr("use_debugger")(debugger);

          if (!workbookModulePattern.empty())
            theWorkbookOpenHandler = createWorkbookOpenHandler(theCoreAddinContext);

          getAddins().emplace(context->pathName(), theCoreAddinContext);
          return 0;
        }

        case PluginContext::Attach:
        {
          const bool useLoaderThread = plugin.settings["UseLoaderThread"].value_or(true);

          // Attach is called for each XLL which uses xlOil_Python.
          assert(context);
          auto pyContext = make_shared<PyAddin>(
            *context, 
            plugin.settings["SeparateThread"].value_or(false),
            plugin.settings["ComLib"].value_or(DEFAULT_COM_BINDER),
            std::wstring_view(),
            useLoaderThread);

          // Set sys.path to include the attaching addin
          {
            py::gil_scoped_acquire gilAcquired;
            appendSysPath(addinDir(*context));
          }

          // Load any modules requested in the settings file
          vector<string> modsToLoad;
          auto loadModules = plugin.settings["LoadModules"].as_array();
          if (loadModules)
            for (auto& m : *loadModules)
              modsToLoad.push_back(m.value_or<string>(""));

          // Given a pattern like *.py, try to find a python module of the form <xll-name>.py
          auto addinModule = plugin.settings["AddinModule"].value_or(string("*.py"));
          auto star = addinModule.find('*');
          if (star != string::npos)
          {
            auto filename = utf8ToUtf16(addinModule);
            // Replace the star with our addin name (minus the extension)
            auto xllPath = fs::path(context->pathName()).replace_extension("");
            filename.replace(filename.find(L'*'), 1u, xllPath);

            std::error_code err;
            if (fs::exists(filename, err))
              modsToLoad.push_back(xllPath.stem().string());
          }

          {
            py::gil_scoped_acquire gilAcquired;
            pyContext->importModule(py::cast(modsToLoad), useLoaderThread);
          }

          getAddins().emplace(context->pathName(), pyContext);
          return 0;
        }

        case PluginContext::Detach:
        {
          // On detach, remove the PyAddin object. Functions registered by
          // the addin will be remove by xlOil machinery
          auto found = getAddins().find(context->pathName());
          if (found != getAddins().end())
          {
            found->second->unload();
            getAddins().erase(found);
          }
          return 0;
        }

        case PluginContext::Unload:
          theWorkbookOpenHandler.reset();
          getAddins().clear();
          theCoreAddinContext.reset();
          exit();
          return 0;
        }
      }
      catch (const std::exception& e)
      {
        if (Py_IsInitialized())
        {
          try
          {
            py::gil_scoped_acquire getGil;
            // Give enviroment details as most problems at this stage are path related
            XLO_ERROR("xloil_python_init failed: {0}.\n{1}\n", e.what(), getPythonEnvDetails());
          }
          catch (...)
          {}
        }
        else
          XLO_ERROR("xloil_python_init failed: {0}", e.what());
      }
      catch (...)
      {
        // Returning -1 will trigger a log entry by the caller, so no need to write one here
      }
      return -1;
    }
  }
}