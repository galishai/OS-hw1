#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
	// TODO: Add your implementation
    cout << "smash: got ctrl-Z" << endl;
    SmallShell& smash = SmallShell::getInstance();
    if(smash.fg_pid != -1)
    {
        return;
    }
    smash.jobs_list.addJob(smash.cmd,smash.fg_pid,true);
    kill(smash.fg_pid, SIGSTOP);
    cout << "smash: process " << smash.fg_pid <<" was stopped" << endl;
    smash.fg_pid = -1;
    smash.fg_cmdline = "";
}

void ctrlCHandler(int sig_num) {
  // TODO: Add your implementation
    cout << "smash: got ctrl-C" << endl;
    SmallShell& smash = SmallShell::getInstance();
    if(smash.fg_pid != -1)
    {
        return;
    }
    kill(smash.fg_pid, SIGKILL);
    cout << "smash: process " << smash.fg_pid <<" was killed" << endl;
    smash.fg_pid = -1;
    smash.fg_cmdline = "";
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation

}
