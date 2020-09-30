#include "Log.hh"
#include "Util.hh"

sighandler signalNoRestart(int signum, sighandler handler)
{
    struct sigaction action, oldAction;
    char errbuf[256];

    action.sa_sigaction = handler; 
    // block all signal(except SIGTERM) here. We use SIGTERM instead of SIGKILL
    // as a final way of closing a program.
    sigfillset(&action.sa_mask);
    sigdelset(&action.sa_mask, SIGTERM);
    action.sa_flags = 0;

    if (sigaction(signum, &action, &oldAction) < 0)
    {
        log.error("signalNoRestart: sigaction() failed(%s).", 
            Log::strerror(errbuf));
        exit(1);
    }
    return oldAction.sa_sigaction;
}
