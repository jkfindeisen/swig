/* -----------------------------------------------------------------------------
 * main.cxx
 *
 *     Main entry point to the SWIG core.
 *
 * Author(s) : David Beazley (beazley@cs.uchicago.edu)
 *
 * Copyright (C) 1998-2000.  The University of Chicago
 * Copyright (C) 1995-1998.  The University of Utah and The Regents of the
 *                           University of California.
 *
 * See the file LICENSE for information on usage and redistribution.
 * ----------------------------------------------------------------------------- */

static char cvsroot[] = "$Header$";

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "swig11.h"
#ifndef MACSWIG
#include "swigconfig.h"
#endif

extern "C" {
#include "preprocessor.h"
}

#include <ctype.h>

#ifndef SWIG_LIB
#define SWIG_LIB "/usr/local/lib/swig1.3"
#endif

#ifndef SWIG_CC
#define SWIG_CC "CC"
#endif

// Global variables

    char       LibDir[512];                      // Library directory
    int        ReadOnly;                        // Read-only mode
    Language  *lang;                            // Language method
    int        CPlusPlus = 0;
    int        AddMethods = 0;                  // AddMethods flag
    int        NewObject = 0;                   // NewObject flag
    int        ForceExtern = 0;                 // Force extern mode
    int        GenerateDefault = 1;             // Generate default constructors
    char      *Config = 0;
    int        NoInclude = 0;

    int        error_count = 0;                 // Error count
    int        Verbose = 0;

extern String  *ModuleName;
extern int ShowTemplates;

static char *usage = (char*)"\
\nGeneral Options\n\
     -c              - Produce raw wrapper code (omit support code)\n\
     -c++            - Enable C++ processing\n\
     -co             - Check a file out of the SWIG library\n\
     -Dsymbol        - Define a symbol (for conditional compilation)\n\
     -I<dir>         - Look for SWIG files in <dir>\n\
     -includeall     - Follow all #include statements\n\
     -importall      - Follow all #include statements as imports\n\
     -l<ifile>       - Include SWIG library file.\n\
     -make_default   - Create default constructors/destructors (the default)\n\
     -no_default     - Do not generate constructors/destructors\n\
     -module         - Set module name\n\
     -o outfile      - Set name of the output file.\n\
     -swiglib        - Report location of SWIG library and exit\n\
     -v              - Run in verbose mode\n\
     -version        - Print SWIG version number\n\
     -help           - This output.\n\n";

// -----------------------------------------------------------------------------
// check_suffix(char *name)
//
// Checks the suffix of a file to see if we should emit extern declarations.
// -----------------------------------------------------------------------------

int
check_suffix(char *name) {
  char *c;
  if (!name) return 0;
  c = Swig_file_suffix(name);
  if ((strcmp(c,".c") == 0) ||
      (strcmp(c,".C") == 0) ||
      (strcmp(c,".cc") == 0) ||
      (strcmp(c,".cxx") == 0) ||
      (strcmp(c,".c++") == 0) ||
      (strcmp(c,".cpp") == 0)) {
    return 1;
  }
  return 0;
}

// -----------------------------------------------------------------------------
// install_opts(int argc, char *argv[])
// Install all command line options as preprocessor symbols
// ----------------------------------------------------------------------------- 

static void
install_opts(int argc, char *argv[]) {
  int i;
  int noopt = 0;
  char *c;
  for (i = 1; i < (argc-1); i++) {
    if (argv[i]) {
      if ((*argv[i] == '-') && (!isupper(*(argv[i]+1)))) {
	String *opt = NewStringf("SWIGOPT%(upper)s", argv[i]);
	Replaceall(opt,"-","_");
	c = Char(opt);
	noopt = 0;
	while (*c) {
	  if (!(isalnum(*c) || (*c == '_'))) {
	    noopt = 1;
	    break;
	  }
	  c++;
	}
	if (((i+1) < (argc-1)) && (argv[i+1]) && (*argv[i+1] != '-')) {
	  Printf(opt," %s", argv[i+1]);
	  i++;
	} else {
	  Printf(opt," 1");
	}
	if (!noopt) {
	  /*	  Printf(stdout,"%s\n", opt); */
	  Preprocessor_define(opt, 0);
	}
      }
    }
  }
}
//-----------------------------------------------------------------
// main()
//
// Main program.    Initializes the files and starts the parser.
//-----------------------------------------------------------------

char *SwigLib;
static int     freeze = 0;
static String  *lang_config = 0;

/* This function sets the name of the configuration file */

void SWIG_config_file(const String_or_char *filename) {
  lang_config = NewString(filename);
}

void SWIG_library_directory(const char *filename) {
  strcpy(LibDir,filename);
}

extern  "C" Node *Swig_cparse(File *);

int SWIG_main(int argc, char *argv[], Language *l) {
  int    i;
  char   *c;
  char    temp[512];
  char   *outfile_name = 0;
  int     help = 0;
  int     checkout = 0;
  int     cpp_only = 0;
  int     tm_debug = 0;
  char   *includefiles[256];
  int     includecount = 0;
  extern  int check_suffix(char *);
  int     dump_tags = 0;
  int     dump_tree = 0;
  int     contracts = 0;
  int     browse = 0;

  DOH    *libfiles = 0;
  DOH    *cpps = 0 ;
  extern  void Swig_contracts(Node *n);
  extern void Swig_browser(Node *n, int);
  extern  void Swig_default_allocators(Node *n);

  /* Initialize the SWIG core */
  Swig_init();
  
  // Initialize the preprocessor
  Preprocessor_init();

  lang = l;
  ReadOnly = 0;

  // Set up some default symbols (available in both SWIG interface files
  // and C files)

  Preprocessor_define((DOH *) "SWIG 1", 0);
  Preprocessor_define((DOH *) "__STDC__", 0);
#ifdef MACSWIG
  Preprocessor_define((DOH *) "SWIGMAC 1", 0);
#endif
#ifdef SWIGWIN32
  Preprocessor_define((DOH *) "SWIGWIN32 1", 0);
#endif

  // Set the SWIG version value
  String *vers;
  vers = NewStringf("SWIG_VERSION 0x%02d%02d%02d", SWIG_MAJOR_VERSION, SWIG_MINOR_VERSION, SWIG_SPIN);
  Preprocessor_define(vers,0);

  // Check for SWIG_LIB environment variable

  if ((c = getenv("SWIG_LIB")) == (char *) 0) {
#if defined(_WIN32)
      char buf[MAX_PATH];
      char *p;
      if (GetModuleFileName(0, buf, MAX_PATH) == 0
	  || (p = strrchr(buf, '\\')) == 0) {
       Printf(stderr, "Warning: Could not determine SWIG library location. Assuming " SWIG_LIB "\n");
       sprintf(LibDir,"%s",SWIG_LIB);    // Build up search paths
      } else {
       strcpy(p+1, "Lib");
       strcpy(LibDir, buf);
      }
#else
       sprintf(LibDir,"%s",SWIG_LIB);    // Build up search paths
#endif                                        
  } else {
      strcpy(LibDir,c);
  }

  SwigLib = Swig_copy_string(LibDir);        // Make a copy of the real library location

  sprintf(temp,"%s%sconfig", LibDir, SWIG_FILE_DELIMETER);
  Swig_add_directory((DOH *) temp);
  Swig_add_directory((DOH *) "." SWIG_FILE_DELIMETER "swig_lib" SWIG_FILE_DELIMETER "config");
  Swig_add_directory((DOH *) LibDir);
  Swig_add_directory((DOH *) "." SWIG_FILE_DELIMETER "swig_lib");
  
  libfiles = NewList();

  // Get options
  for (i = 1; i < argc; i++) {
      if (argv[i]) {
	  if (strncmp(argv[i],"-I",2) == 0) {
	    // Add a new directory search path
	    includefiles[includecount++] = Swig_copy_string(argv[i]+2);
	    Swig_mark_arg(i);
	  } else if (strncmp(argv[i],"-D",2) == 0) {
	    DOH *d = NewString(argv[i]+2);
	    Replace(d,(char*)"=",(char*)" ", DOH_REPLACE_ANY | DOH_REPLACE_FIRST);
	    Preprocessor_define((DOH *) d,0);
	    // Create a symbol
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-E") == 0) {
	    cpp_only = 1;
	    Swig_mark_arg(i);
	  } else if ((strcmp(argv[i],"-verbose") == 0) ||
		     (strcmp(argv[i],"-v") == 0)) {
	      Verbose = 1;
	      Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-c++") == 0) {
	      CPlusPlus=1;
	      Preprocessor_define((DOH *) "__cplusplus 1", 0);
	      Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-c") == 0) {
	      NoInclude=1;
	      Preprocessor_define((DOH *) "SWIG_NOINCLUDE 1", 0);
	      Swig_mark_arg(i);
          } else if (strcmp(argv[i],"-make_default") == 0) {
	    GenerateDefault = 1;
	    Swig_mark_arg(i);
          } else if (strcmp(argv[i],"-no_default") == 0) {
	    GenerateDefault = 0;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-show_templates") == 0) {
	    ShowTemplates = 1;
	    Swig_mark_arg(i);
          } else if (strcmp(argv[i],"-swiglib") == 0) {
	    printf("%s\n", LibDir);
	    SWIG_exit (EXIT_SUCCESS);
	  } else if (strcmp(argv[i],"-o") == 0) {
	      Swig_mark_arg(i);
	      if (argv[i+1]) {
		outfile_name = Swig_copy_string(argv[i+1]);
		Swig_mark_arg(i+1);
		i++;
	      } else {
		Swig_arg_error();
	      }
	  } else if (strcmp(argv[i],"-version") == 0) {
 	      fprintf(stderr,"\nSWIG Version %s\n",
		      SWIG_VERSION);
	      fprintf(stderr,"Copyright (c) 1995-1998\n");
	      fprintf(stderr,"University of Utah and the Regents of the University of California\n");
              fprintf(stderr,"Copyright (c) 1998-2001\n");
	      fprintf(stderr,"University of Chicago\n");
	      fprintf(stderr,"\nCompiled with %s\n", SWIG_CC);
	      SWIG_exit (EXIT_SUCCESS);
	  } else if (strncmp(argv[i],"-l",2) == 0) {
	    // Add a new directory search path
	    Append(libfiles,argv[i]+2);
	    Swig_mark_arg(i);
          } else if (strcmp(argv[i],"-co") == 0) {
	    checkout = 1;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-freeze") == 0) {
	    freeze = 1;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-includeall") == 0) {
	    Preprocessor_include_all(1);
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-importall") == 0) {
	    Preprocessor_import_all(1);
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-tm_debug") == 0) {
	    tm_debug = 1;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-module") == 0) {
	    Swig_mark_arg(i);
	    if (argv[i+1]) {
	      ModuleName = NewString(argv[i+1]);	    
	      Swig_mark_arg(i+1);
	    } else {
	      Swig_arg_error();
	    }
	  } else if (strcmp(argv[i],"-dump_tags") == 0) {
	    dump_tags = 1;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-dump_tree") == 0) {
	    dump_tree = 1;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-contracts") == 0) {
	    Swig_mark_arg(i);
	    contracts = 1;
	  } else if (strcmp(argv[i],"-browse") == 0) {
	    browse = 1;
	    Swig_mark_arg(i);
	  } else if (strcmp(argv[i],"-help") == 0) {
	    fputs(usage,stderr);
	    Swig_mark_arg(i);
	    help = 1;
	  }
      }
  }
  if (Verbose)
    printf ("LibDir: %s\n", LibDir);

  while (includecount > 0) {
    Swig_add_directory((DOH *) includefiles[--includecount]);
  }

  // Define the __cplusplus symbol
  if (CPlusPlus)
    Preprocessor_define((DOH *) "__cplusplus 1", 0);

  // Parse language dependent options
  lang->main(argc,argv);

  if (help) SWIG_exit (EXIT_SUCCESS);    // Exit if we're in help mode

  // Check all of the options to make sure we're cool.
  Swig_check_options();

  install_opts(argc, argv);

  // Add language dependent directory to the search path
  {
    DOH *rl = NewString("");
    Printf(rl,"%s%s%s", SwigLib, SWIG_FILE_DELIMETER, LibDir);
    Swig_add_directory(rl);
  }

  // If we made it this far, looks good. go for it....

  input_file = argv[argc-1];

  // If the user has requested to check out a file, handle that
  if (checkout) {
    DOH *s;
    char *outfile = input_file;
    if (outfile_name)
      outfile = outfile_name;

    if (Verbose)
      printf ("Handling checkout...\n");

    s = Swig_include(input_file);
    if (!s) {
      fprintf(stderr,"Unable to locate '%s' in the SWIG library.\n", input_file);
    } else {
      FILE *f = fopen(outfile,"r");
      if (f) {
	fclose(f);
	fprintf(stderr,"File '%s' already exists. Checkout aborted.\n", outfile);
      } else {
	f = fopen(outfile,"w");
	if (!f) {
	  fprintf(stderr,"Unable to create file '%s'\n", outfile);
	} else {
	  fprintf(stderr,"'%s' checked out from the SWIG library.\n", input_file);
	  fputs(Char(s),f);
	  fclose(f);
	}
      }
    }
  } else {
    // Check the suffix for a .c file.  If so, we're going to
    // declare everything we see as "extern"

    ForceExtern = check_suffix(input_file);

    // Run the preprocessor
    if (Verbose)
      printf ("Preprocessing...\n");
    {
      int i;
      String *fs = NewString("");
      FILE *df = Swig_open(input_file);
      if (!df) {
	Printf(stderr,"Unable to find '%s'\n", input_file);
	SWIG_exit (EXIT_FAILURE);
      }
      fclose(df);
      Printf(fs,"%%include \"swig.swg\"\n");
      if (lang_config) {
	Printf(fs,"\n%%include \"%s\"\n", lang_config);
      }
      Printf(fs,"%%include \"%s\"\n", Swig_last_file());
      for (i = 0; i < Len(libfiles); i++) {
	Printf(fs,"\n%%include \"%s\"\n", Getitem(libfiles,i));
      }
      Seek(fs,0,SEEK_SET);
      cpps = Preprocessor_parse(fs);
      if (Preprocessor_errors()) {
	SWIG_exit(EXIT_FAILURE);
      }
      if (cpp_only) {
	Printf(stdout,"%s", cpps);
	while (freeze);
	SWIG_exit (EXIT_SUCCESS);
      }
      Seek(cpps, 0, SEEK_SET);
    }

    /* Register a null file with the file handler */
    Swig_register_filebyname("null", NewString(""));

    // Pass control over to the specific language interpreter
    if (Verbose) {
      fprintf (stdout, "Starting language-specific parse...\n");
      fflush (stdout);
    }
    Node *top = Swig_cparse(cpps);
    Swig_default_allocators(top);

    if (dump_tags) {
      Swig_dump_tags(top,0);
    }
    if (dump_tree) {
      Swig_dump_tree(top);
    }
    if (top) {
      if (!Getattr(top,"name")) {
	Printf(stderr,"*** No module name specified using %%module or -module.\n");
	SWIG_exit(EXIT_FAILURE);
      } else {
	/* Set some filename information on the object */
	Setattr(top,"infile", input_file);
	if (!outfile_name) {
	  if (CPlusPlus) {
	    Setattr(top,"outfile", NewStringf("%s_wrap.cxx", Swig_file_basename(input_file)));
	  } else {
	    Setattr(top,"outfile", NewStringf("%s_wrap.c", Swig_file_basename(input_file)));
	  }
	} else {
	  Setattr(top,"outfile", outfile_name);
	}
	if (contracts) {
	  Swig_contracts(top);
	}
	lang->top(top);
	if (browse) {
	  Swig_browser(top,0);
	}
      }
    }
  }
  if (tm_debug) Swig_typemap_debug();
  while (freeze);
  return error_count;
}

// --------------------------------------------------------------------------
// SWIG_exit(int exit_code)
//
// Cleanup and either freeze or exit
// --------------------------------------------------------------------------

void SWIG_exit(int exit_code) {
  while (freeze);
  exit (exit_code);
}

