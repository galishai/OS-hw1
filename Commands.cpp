#include <unistd.h>
#include <string>
#include <iostream>
#include <vector>
#include <sstream>

#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>

using namespace std;
const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()

#define OVERWRITE 1
#endif

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}
//receives cmd_line and an empty array of strings args. Separates the words in cmd_line into different array slots in args. Returns the number of words in cmdline.
int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h

char** getCmdArgs(const char* cmd_line, int *numOfArgs)
{
    char** argsArray = (char**) malloc(COMMAND_MAX_ARGS);
    if(!argsArray)
    {
        return nullptr;
    }
    for(int i = 0; i < COMMAND_MAX_ARGS; i++)
    {
        argsArray[i] = nullptr;
    }
    *numOfArgs = _parseCommandLine(cmd_line,argsArray);
    return argsArray;
}

bool checkIfNumber(const char* input)
{
    while(*input != '\0')
    {
        if(!isdigit(*input))
        {
            return false;
        }
        input++;
    }
    return true;
}

void freeCmdArgs(char** args)
{
    for(int i = 0; i < COMMAND_MAX_ARGS; i++)
    {
        free(args[i]);
    }
    delete[] args;
}

Command::Command(const char *cmd_line):cmdline(cmd_line){}


BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line)
{

}

ChangePromptCommand::ChangePromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
}

void ChangePromptCommand::execute(){
    int num_of_args;
    char** args = getCmdArgs(this->cmdline,&num_of_args);
    if(args == nullptr)
    {
        perror("smash error: malloc failed");
        return;
    }
    SmallShell &shell = SmallShell::getInstance();
    if(num_of_args == 1)
    {
        shell.prompt = "smash";
    }
    else
    {
        shell.prompt = args[1];
    }
    for(int i = 0; i < COMMAND_ARGS_MAX_LENGTH; i++)
    {
        free(args[i]);
    }
    free(args);
}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{}

void ShowPidCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();
    cout << "smash pid is " << smash.shell_pid << endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line): BuiltInCommand(cmd_line)
{}

void GetCurrDirCommand::execute()
{
    char* buffer = new char[WD_MAX_LENGTH+1];
    if(getcwd(buffer, WD_MAX_LENGTH+1) == nullptr)
    {
        delete[] buffer;
        perror("CWD size exceeds limit");
        return;
    }
    cout << buffer << endl;
    delete[] buffer;
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), last_wd(plastPwd)
{}

void ChangeDirCommand::execute()
{
    int num_of_args;
    char** args = getCmdArgs(cmdline,&num_of_args);
    if(num_of_args > 2)
    {
        perror("smash error: cd: too many arguments"); //TODO
        return;
    }
    if(num_of_args == 1) //TODO
    {
        perror("smash error: cd: no arguments specified");
        return;
    }
    if(strcmp(args[1],"-") == 0)
    {
        SmallShell &smash = SmallShell::getInstance();
        if(!(*last_wd))
        {
            perror("smash error: cd: OLDPWD not set");
        }
        if(chdir(*last_wd) == -1)
        {
            perror("smash error: chdir failed");
        }
    }
}

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}

void JobsCommand::execute()
{
    jobs->removeFinishedJobs();
    jobs->printJobsList();
}

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}

void ForegroundCommand::fgHelper(JobsList::JobEntry* job)
{
    if(job->stopped)
    {
        if (kill(job->pid, SIGCONT) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
    }
    cout << cmdline << " : " << job->pid << endl;

    SmallShell& smash = SmallShell::getInstance();
    smash.fg_cmdline = cmdline;
    //smash.fg_job_id = job->job_id;
    smash.fg_pid = job->pid;
    jobs->removeJobById(job->job_id);
    if(waitpid(job->pid, nullptr, WUNTRACED) == -1) //TODO check if WUNTRACED is needed
    {
        perror("smash error: waitpid failed");
        return;
    }
}

void ForegroundCommand::execute()
{
    int num_of_args;
    char** args = getCmdArgs(cmdline,&num_of_args);
    if(num_of_args > 2)
    {
        perror("smash error: fg: invalid arguments");
        freeCmdArgs(args);
        return;
    }
    if(num_of_args == 2)
    {
        if(!checkIfNumber(args[1]))
        {
            perror("smash error: fg: invalid arguments");
            freeCmdArgs(args);
            return;
        }
        JobsList::JobEntry* job = jobs->getJobByID(stoi(args[1]));
        if(!job)
        {
            cerr << "smash error: fg: job-id " << args[1] << " does not exist" << endl;
            freeCmdArgs(args);
            return;
        }
        fgHelper(job);
    }
    if(num_of_args == 1)
    {
        if(SmallShell::getInstance().jobs_list.list_of_jobs.empty())
        {
            perror("smash error: fg: jobs list is empty");
            freeCmdArgs(args);
            return;
        }
        JobsList::JobEntry* job = jobs->getLastJob();
        fgHelper(job);
    }
    freeCmdArgs(args);
}

void BackgroundCommand::bgHelper(JobsList::JobEntry* job){
    if (kill(job->pid, SIGCONT) == -1)
    {
        perror("smash error: kill failed");
        return;
    }
    job->stopped = false;
    cout << cmdline << " : " << job->pid << endl;
}

void BackgroundCommand::execute()
{
    int num_of_args;
    char** args = getCmdArgs(cmdline,&num_of_args);
    if(num_of_args > 2)
    {
        perror("smash error: bg: invalid arguments");
        freeCmdArgs(args);
        return;
    }
    if(num_of_args == 2)
    {
        if (!checkIfNumber(args[1]))
        {
            perror("smash error: bg: invalid arguments");
            freeCmdArgs(args);
            return;
        }
        JobsList::JobEntry* job = jobs->getJobByID(stoi(args[1]));
        if(!job)
        {
            cerr << "smash error: bg: job-id " << args[1] << " does not exist" << endl;
            freeCmdArgs(args);
            return;
        }
        if(!job->stopped) //if running in bg
        {
            cerr << "smash error: bg: job-id " << job->job_id << "is already running in the background" << endl;
            freeCmdArgs(args);
            return;
        }
        bgHelper(job);
    }
    if(num_of_args == 1)
    {
        JobsList::JobEntry* last_stopped = jobs->getLastStoppedJob();
        if(last_stopped == nullptr)
        {
            perror("smash error: bg: there is no stopped jobs to resume");
            freeCmdArgs(args);
            return;
        }
        bgHelper(last_stopped);
    }
    freeCmdArgs(args);
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}

void QuitCommand::execute()
{
    int num_of_args;
    char** args = getCmdArgs(cmdline,&num_of_args);
    if(num_of_args > 1 && strcmp(args[1], "kill") == 0)
    {
        jobs->killAllJobs();
    }
    freeCmdArgs(args);
    exit(0);
}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{
}



void KillCommand::execute()
{
    int num_of_args;
    char** args = getCmdArgs(cmdline,&num_of_args);
    char* first_arg = &args[1][1];
    if(num_of_args != 3)
    {
        perror("smash error: kill: invalid arguments");
        freeCmdArgs(args);
        return;
    }
    string signal = string (args[1]).substr(1, string (args[1]).size());
    if(args[1][0] != '-' || !checkIfNumber(signal.c_str()) ||
            !checkIfNumber(args[2]))
    {
        perror("smash error: kill: invalid arguments");
        freeCmdArgs(args);
        return;
    }
    JobsList::JobEntry* job = jobs->getJobByID(stoi(args[2]));
    if(job == nullptr)
    {
        cerr << "smash error: kill: job-id " << args[2] <<" does not exist" << endl;
        freeCmdArgs(args);
        return;
    }
    if(kill(job->pid, stoi(signal) == -1))
    {
        perror("smash error: kill failed");
        freeCmdArgs(args);
        return;
    }
    if(stoi(signal) == SIGCONT)
    {
        job->stopped = false;
    }
    if(stoi(signal) == SIGSTOP)
    {
        job->stopped = true;
    }
    cout << "signal number " << signal << " was sent to pid " << job->pid << endl;
}

SmallShell::SmallShell() : jobs_list()
{
    prompt = "smash";
    fg_pid; //TODO to init or not?
    fg_cmdline; //
    //fg_job_id; //
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:
/*
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if ...
  .....
  else {
    return new ExternalCommand(cmd_line);
  }
  */
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  // Command* cmd = CreateCommand(cmd_line);
  // cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}


ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line)
{}

void ExternalCommand::execute()
{
    int num_of_args;
    char** args = getCmdArgs(cmdline,&num_of_args);
    bool isComplex = false;
    const char* temp = cmdline;
    while(*temp)
    {
        if(*temp == '?' || *temp == '*')
        {
            isComplex = true;
        }
        temp++;
    }
    char path[] = "/bin/bash";
    char flag[] = "-c";
    char* complex_args[] = {path, flag, const_cast<char *>(cmdline), nullptr};
    pid_t pid = fork();
    if(pid == -1)
    {
        perror("smash error: fork failed");
        freeCmdArgs(args);
        return;
    }
        if(pid == 0) //son
        {
            if(setpgrp() == -1)
            {
                perror("smash error: setpgrp failed");
                freeCmdArgs(args);
                return;
            }
            if(!isComplex)
            {
                if(execvp(args[0],args) ==-1)
                {
                    perror("smash error: execvp failed");
                    exit(-1);
                }
            }
            else
            {
                if(execvp(path, complex_args) == -1)
                {
                    perror("smash error: execvp failed");
                    exit(-1);
                }
            }
        }
        else //parent
        {
            string copy = string (cmdline);
            SmallShell &smash = SmallShell::getInstance() ;
            if (copy.find_last_not_of(' ') == '&') //if background command
            {
                smash.jobs_list.addJob(this,pid,false);
            }
            else
            {

                smash.fg_cmdline = cmdline;
                smash.fg_pid = pid;
                if(waitpid(pid, nullptr, WUNTRACED) == -1) //TODO check if WUNTRACED is needed
                {
                    perror("smash error: waitpid failed");
                    return;
                }
                smash.fg_cmdline = nullptr;
                smash.fg_pid = -1;
            }
        }
}

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line)
{}

void RedirectionCommand::execute()
{
    int pos1 = string (cmdline).find('>');
    int pos2 = string (cmdline).find(">>");
    int flag;
    if(pos1 == string::npos && pos2 == string::npos)
    {
        perror("Invalid format");
        return;
    }
    SmallShell& smash = SmallShell::getInstance();
    string cmd;
    string out;
    if(pos2 == string::npos) //if >
    {
        flag = OVERWRITE; // >
        cmd = string(cmdline).substr(0, pos1);
        cmd = _trim(cmd);
        out = string(cmdline).substr(pos1 + 1, string(cmdline).length());
        out = _trim(out);
    }
    else
    {
        flag = 0; // >>
        cmd = string(cmdline).substr(0,pos1);
        cmd = _trim(cmd);
        cmd = string (cmdline).substr(pos1+2, string(cmdline).length());
        cmd = _trim(out);
    }
    Command* cmd_exe = smash.CreateCommand(cmd.c_str());
    pid_t pid = fork();
    if(pid == -1)
    {
        perror("smash error: fork failed");
    }
    if(pid == 0)
    {
        if (close(1) == -1)
        {
            perror("smash error: close failed");
            exit(1);
        }
        if(flag == OVERWRITE)
        {
            if(open("output.txt", O_WRONLY, O_CREAT) == -1)
            {
                perror("smash error: open failed");
                exit(1);
            }
        }else
        {
            if(open("output.txt", O_WRONLY, O_CREAT, O_APPEND) == -1)
            {
                perror("smash error: open failed");
                exit(1);
            }
        }
        cmd_exe->execute();
        if(!_isBackgroundComamnd(cmd.c_str()))
        {
            delete cmd_exe;
        }

    }else
    {
        if (waitpid(pid, nullptr, WUNTRACED) == -1)
        {
            perror("smash error: waitpid failed");
        }
    }
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line)
{}

bool closePipe(int fd[])
{
    if(close(fd[0]) == -1 || close(fd[1] == -1))
    {
        return false;
    }
    return true;
}

void PipeCommand::execute()
{
    int pos1 = string (cmdline).find('|');
    int pos2 = string (cmdline).find("|&");
    int flag;
    if(pos1 == string::npos && pos2 == string::npos)
    {
        perror("Invalid format");
        return;
    }
    SmallShell& smash = SmallShell::getInstance();
    string cmd1;
    string cmd2;
    if(pos2 == string::npos) //if |
    {
        flag=1;
        cmd1 = string(cmdline).substr(0, pos1);
        cmd1 = _trim(cmd1);
        cmd2 = string(cmdline).substr(pos1 + 1, string(cmdline).length());
        cmd2 = _trim(cmd2);
    }
    else
    {
        flag=2;
        cmd1 = string(cmdline).substr(0,pos1);
        cmd1 = _trim(cmd1);
        cmd2 = string (cmdline).substr(pos1+2, string(cmdline).length());
        cmd2 = _trim(cmd2);
    }
        int fd[2];
        if(pipe(fd) == -1)
        {
            perror("smash error: pipe failed");
            exit(1);
        }
        int pid1 = fork();
        if(pid1 == -1)
        {
            perror("smash error: fork failed");
            if(!closePipe(fd))
            {
                perror("smash error: close failed");
            }
            exit(1);
        }
        if(pid1 == 0)
        {
            if(setpgrp() == -1)
            {
                perror("smash error: setpgrp failed");
                if(!closePipe(fd))
                {
                    perror("smash error: close failed");
                }
                exit(1);
            }
            if(dup2(fd[1],flag) == -1)
            {
                perror("smash error: dup2 failed");
                if(!closePipe(fd))
                {
                    perror("smash error: close failed");
                }
                exit(1);
            }
            Command* cmd1_exe = smash.CreateCommand(cmd1.c_str());
            cmd1_exe->execute();
            if(!_isBackgroundComamnd(cmd1.c_str()))
            {
                delete cmd1_exe;
            }
        }
            if (waitpid(pid1, nullptr, WUNTRACED) == -1) //TODO where to put waitpids
            {
                perror("smash error: waitpid failed");
                return;
            }
        int pid2 = fork();
        if(pid2 == -1)
        {
            perror("smash error: fork failed");
            if(!closePipe(fd))
            {
                perror("smash error: close failed");
            }
            exit(1);
        }

        if(pid2 == 0)
        {
            if(setpgrp() == -1)
            {
                perror("smash error: setpgrp failed");
                if(!closePipe(fd))
                {
                    perror("smash error: close failed");
                }
                exit(1);
            }
            if(dup2(fd[0],0) == -1)
            {
                perror("smash error: dup2 failed");
                if(!closePipe(fd))
                {
                    perror("smash error: close failed");
                }
                exit(1);
            }
            Command* cmd2_exe = smash.CreateCommand(cmd2.c_str());
            cmd2_exe->execute();
            if(!_isBackgroundComamnd(cmd2.c_str()))
            {
                delete cmd2_exe;
            }
        }
        if(!closePipe(fd))
        {
            perror("smash error: close failed");
        }
        if(waitpid(pid2, nullptr,WUNTRACED) == -1)
        {
            perror("smash error: waitpid failed");
            return;
        }
}

SetcoreCommand::SetcoreCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{}

void SetcoreCommand::execute()
{
    int num_of_args;
    char** args = getCmdArgs(cmdline,&num_of_args);
    if(num_of_args != 3 || !checkIfNumber(args[1]) || !checkIfNumber(args[2]))
    {
        perror("smash error: setcore: invalid arguments");
        freeCmdArgs(args);
        return;
    }
    SmallShell& smash = SmallShell::getInstance();
    JobsList::JobEntry* job = smash.jobs_list.getJobByID(stoi(args[1]));
    if(job == nullptr)
    {
        cerr << "smash error: setcore: job-id " << args[1] << " does not exist" << endl;
        freeCmdArgs(args);
        return;
    }
    int core_num = sysconf(_SC_NPROCESSORS_ONLN);
    if(core_num == -1)
    {
        perror("smash error: sysconf failed");
        freeCmdArgs(args);
        return;
    }
    if(stoi(args[2]) < 0 || core_num <= stoi(args[2]))
    {
        perror("smash error: setcore: invalid core number");
        freeCmdArgs(args);
        return;
    }
    cpu_set_t mask;
    sched_setaffinity

}




//TODO JobsList Functions:

JobsList::JobsList():list_of_jobs(), nextJobID(1)
{
}

JobsList::JobEntry::JobEntry(bool stopped, int job_id, const char* cmd_line, pid_t pid):finished(false),stopped(stopped),job_id(job_id), time_added(-1), cmd_line(cmd_line), pid(pid)
{}

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped)
{
    JobsList::removeFinishedJobs();
    JobEntry new_job(isStopped,nextJobID, cmd->cmdline, pid);
    nextJobID++;
    new_job.time_added = time(nullptr);
    list_of_jobs.push_back(new_job);

}

void JobsList::printJobsList() //TODO check space validity
{
    for(const auto& job : list_of_jobs)
    {
        if(!job.finished)
        {
            if (!job.stopped)
            {
                cout << "[" << job.job_id << "]" << job.cmd_line << " : " << job.pid << " "
                     << (int) difftime(job.time_added,
                                       time(nullptr)) << " secs" << endl;
            }
            else
            {
                cout << "[" << job.job_id << "]" << job.cmd_line << " : " << job.pid << " "
                     << (int) difftime(job.time_added,
                                       time(nullptr)) << " secs (stopped)" << endl;
            }
        }
    }
}

void JobsList::removeFinishedJobs()
{
    list_of_jobs.remove_if([](JobEntry job) {return job.finished;}); //TODO check validity
}

JobsList::JobEntry* JobsList::getJobByID(int jobId)
{
    for(auto& job : list_of_jobs)
    {
        if(job.job_id == jobId)
        {
            return &job;
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId)
{
    list_of_jobs.remove_if([&jobId](JobEntry job) {if(job.job_id == jobId) return true;}); //TODO check validity
}

JobsList::JobEntry *JobsList::getLastJob()
{
    return &list_of_jobs.back();
}

JobsList::JobEntry* JobsList::getLastStoppedJob()
{
    JobEntry* last_stopped = nullptr;

    for(auto& job : list_of_jobs)
    {
        if(job.stopped)
        {
            last_stopped = &job;
        }
    }
    return last_stopped;
}

void JobsList::killAllJobs()
{
    cout << " sending SIGKILL signal to "<< list_of_jobs.size() << " jobs" << endl;
    for(auto& job : list_of_jobs)
    {
        cerr << job.pid << ": " << job.cmd_line << endl;
    }
    for(auto& job : list_of_jobs)
    {

        if(kill(job.pid,SIGKILL) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
    }
}

