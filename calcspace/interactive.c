/*
 *   _____     _     _____                 
 *  |     |___| |___|   __|___ ___ ___ ___ 
 *  |   --| .'| |  _|__   | . | .'|  _| -_|
 *  |_____|__,|_|___|_____|  _|__,|___|___|
 *                        |_|              
 *  (c) fG!, 2012 - reverser@put.as
 *
 *  interactive.c
 *  
 *  Editline related functions
 *
 *  Most of the functions from editline/readline fileman.c example
 *  
 */

#include "interactive.h"

typedef struct {
    uint8_t *targetBuffer;
    options_t *options;
    char *arg;
} cmd_options_t;

enum cmd_type { CMD, CONFIG };

typedef int rl_icpfunc_t (cmd_options_t);
typedef struct {
    char *name;                   /* User printable name of the function. */
    rl_icpfunc_t *func;           /* Function to call to do the job. */
    char *doc;                    /* Documentation for this function.  */
    enum cmd_type type;                 /* type of command: 0-cmd, 1-configuration */
} COMMAND;

static int cmd_quit(cmd_options_t cmd_options);
static int cmd_new(cmd_options_t cmd_options);
static int cmd_free(cmd_options_t cmd_options);
static int cmd_excel(cmd_options_t cmd_options);
static int cmd_all(cmd_options_t cmd_options);
static int cmd_ios(cmd_options_t cmd_options);
static int cmd_help(cmd_options_t cmd_options);

COMMAND commands[] = {
    { "quit",  cmd_quit,  "Exit this utility.", CMD },
    { "exit",  cmd_quit,  "Exit this utility.", CMD },
    { "new",   cmd_new,   "Calculate free space for new commands.", CMD },
    { "free",  cmd_free,  "Calculate free __TEXT space.",CMD },
    { "excel", cmd_excel, "Set output to excel mode.", CONFIG },
    { "all",   cmd_all,   "Set free command to calculate space for all sections.", CONFIG },
    { "ios",   cmd_ios,   "Set target as an iOS application.", CONFIG },
    { "help",  cmd_help,  "Display this text.", CMD },
    { "?",     cmd_help,  "Synonym for `help'.", CMD },
    { (char *)NULL, (rl_icpfunc_t *)NULL, (char *)NULL, CMD }
};


static char * stripwhite (char *string);
static void initialize_readline(void);
static char * dupstr(char* s);
static int execute_line(char *line,const uint8_t *targetBuffer, options_t *options);
static COMMAND *find_command(char *name);

extern void init_options(options_t *options);
extern void reset_options(options_t *options);

int done = 0;

void 
start_interactive_mode(const uint8_t *targetBuffer, options_t *options)
{
    char *line, *s;
    setlocale(LC_CTYPE, "");
    stifle_history(7);
    initialize_readline();
    
    printf(" _____     _     _____ \n");                
    printf("|     |___| |___|   __|___ ___ ___ ___ \n");
    printf("|   --| .'| |  _|__   | . | .'|  _| -_|\n");
    printf("|_____|__,|_|___|_____|  _|__,|___|___|\n");
    printf("                      |_|              \n");
    printf("Calculate free space in mach-o headers\n");
    printf("(c) fG!, 2012 - reverser@put.as\n\n");
    
    for ( ; done == 0; )
    {
        // new line is chomp'ed
        line = readline ("calcspace> ");
        
        if (!line)
            break;
        
        /* Remove leading and trailing whitespace from the line.
         Then, if there is anything left, add it to the history list
         and execute it. */
        s = stripwhite(line);
        
        if (*s) 
        {
            char* expansion;
            int result;
            
            result = history_expand(s, &expansion);
            
            if (result < 0 || result == 2) 
            {
                fprintf(stderr, "%s\n", expansion);
            } 
            else 
            {
                add_history(expansion);
                execute_line(expansion, targetBuffer, options);
            }
            free(expansion);
        }
        free(line);
    }        
}

/* **************************************************************** */
/*                                                                  */
/*                  Interface to Readline Completion                */
/*                                                                  */
/* **************************************************************** */

static char *command_generator(const char *, int);
static char **fileman_completion(const char *, int, int);

/*
 * Tell the GNU Readline library how to complete.  We want to try to
 * complete on command names if this is the first word in the line, or
 * on filenames if not. 
 */
static void
initialize_readline(void)
{
    /* Allow conditional parsing of the ~/.inputrc file. */
    rl_readline_name = "calcspace";
    
    /* Tell the completer that we want a crack first. */
    rl_attempted_completion_function = fileman_completion;
}

/*
 * Attempt to complete on the contents of TEXT.  START and END
 * bound the region of rl_line_buffer that contains the word to
 * complete.  TEXT is the word to complete.  We can use the entire
 * contents of rl_line_buffer in case we want to do some simple
 * parsing.  Returnthe array of matches, or NULL if there aren't any. 
 */
static char **
fileman_completion (const char* text, int start, int end)
{
    char **matches;
    
    matches = (char **)NULL;
    
    /* If this word is at the start of the line, then it is a command
     to complete.  Otherwise it is the name of a file in the current
     directory. */
    if (start == 0)
        matches = completion_matches (text, command_generator);
    
    return (matches);
}

/* 
 * Generator function for command completion.  
 * STATE lets us know whether to start from scratch; without any state
 * (i.e. STATE == 0), then we start at the top of the list. 
 */
static char *
command_generator (text, state)
const char *text;
int state;
{
    static size_t list_index, len;
    char *name;
    
    /* If this is a new word to complete, initialize now.  This
     includes saving the length of TEXT for efficiency, and
     initializing the index variable to 0. */
    if (!state)
    {
        list_index = 0;
        len = strlen(text);
    }
    
    /* Return the next name which partially matches from the
     command list. */
    while ((name = commands[list_index].name))
    {
        list_index++;
        
        if (strncmp (name, text, len) == 0)
            return (dupstr(name));
    }
    
    /* If no names matched, then return NULL. */
    return ((char *)NULL);
}

static char *
dupstr (char* s)
{
    char *r;
    
    r = malloc(strlen (s) + 1);
    strcpy (r, s);
    return (r);
}

/*
 * Strip whitespace from the start and end of STRING.  Return a pointer
 * into STRING. 
 * from fileman.c source
 */
static char *
stripwhite (char *string)
{
    register char *s, *t;
    
    for (s = string; isspace (*s); s++)
        ;
    
    if (*s == 0)
        return (s);
    
    t = s + strlen (s) - 1;
    while (t > s && isspace (*t))
        t--;
    *++t = '\0';
    
    return s;
}

/* 
 * Execute a command line. 
 */
static int
execute_line (char *line,const uint8_t *targetBuffer, options_t *options)
{
    register int i;
    COMMAND *command;
    char *word;
    
    /* Isolate the command word. */
    i = 0;
    while (line[i] && isspace (line[i]))
        i++;
    word = line + i;
    
    while (line[i] && !isspace (line[i]))
        i++;
    
    if (line[i])
        line[i++] = '\0';
    
    command = find_command (word);
    
    if (!command)
    {
        fprintf (stderr, "%s: No such command for calcspace.\n", word);
        return (-1);
    }
    
    cmd_options_t cmd_options;
    cmd_options.targetBuffer = (uint8_t*)targetBuffer;
    cmd_options.options = options;
    
    /* Get argument to command, if any. */
    while (isspace (line[i]))
        i++;
    
    word = line + i;

    cmd_options.arg = word;
    
    return ((*(command->func)) (cmd_options));
}

/*
 * Look up NAME as the name of a command, and return a pointer to that
 * command. 
 * Return a NULL pointer if NAME isn't a command name. 
 */
static COMMAND *
find_command (char *name)
{
    register int i;
    
    for (i = 0; commands[i].name; i++)
        if (strcmp (name, commands[i].name) == 0)
            return (&commands[i]);
    
    return ((COMMAND *)NULL);
}

/* Print out help for ARG, or for all of the commands if ARG is
 not present. */
static int
cmd_help (cmd_options_t cmd_options)
{
    register int i;
    int printed = 0;
    
    // print the help for a specific command
    if (*(cmd_options.arg))
    {
        for (i = 0; commands[i].name; i++)
        {
            if (strcmp (cmd_options.arg, commands[i].name) == 0)
            {
                printf ("%s\t\t%s\n", commands[i].name, commands[i].doc);
                printed++;
            }
        }
    }
    // print all available commands
    else
    {
        // print commands
        printf("Available commands:\n");
        for (i = 0; commands[i].name; i++)
        {
            if ((!*(cmd_options.arg) || (strcmp (cmd_options.arg, commands[i].name) == 0)) && commands[i].type == CMD)
            {
                printf ("%s\t\t%s\n", commands[i].name, commands[i].doc);
                printed++;
            }
        }
        printf("Configuration options:\n");
        for (i = 0; commands[i].name; i++)
        {
            if ((!*(cmd_options.arg) || (strcmp (cmd_options.arg, commands[i].name) == 0)) && commands[i].type == CONFIG)
            {
                printf ("%s\t\t%s\n", commands[i].name, commands[i].doc);
                printed++;
            }
        }
        printf("Note: execute the configuration commands to enable/disable each setting.\n");
    }
    
    if (!printed)
    {
        printf ("No commands match `%s'. Available commands are:\n", cmd_options.arg);
        
        for (i = 0; commands[i].name; i++)
        {
            /* Print in six columns. */
            if (printed == 6)
            {
                printed = 0;
                printf ("\n");
            }
            
            printf ("%s\t", commands[i].name);
            printed++;
        }
        
        if (printed)
            printf ("\n");
    }
    return (0);
}

static int
cmd_quit (cmd_options_t cmd_options)
{
    printf("Bye bye...!\n");
    exit(0);
}

static int
cmd_new (cmd_options_t cmd_options)
{
    cmd_options.options->newCmdsActive = 1;
    process_target(cmd_options.targetBuffer, *(cmd_options.options));
    // reset the structure for the commands values
    // else it would display both commands
    reset_options(cmd_options.options);
    return 0;
}

static int
cmd_free (cmd_options_t cmd_options)
{
    cmd_options.options->freeDataSpace = 1;
    process_target(cmd_options.targetBuffer, *(cmd_options.options));
    // reset the structure for the commands values
    // else it would display both commands
    reset_options(cmd_options.options);
    return 0;
}

static int
cmd_excel (cmd_options_t cmd_options)
{
    cmd_options.options->excelActive = ~cmd_options.options->excelActive;
    return 0;
}

static int
cmd_all (cmd_options_t cmd_options)
{
    cmd_options.options->allSections = ~cmd_options.options->allSections;
    return 0;
}

static int
cmd_ios (cmd_options_t cmd_options)
{
    cmd_options.options->iosActive = ~cmd_options.options->iosActive;
    return 0;
}