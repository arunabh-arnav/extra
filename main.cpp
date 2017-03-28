#include<iostream>
#include<cstdlib>
#include<vector>
#include<queue>
#include<fstream>
#include<sstream>
using namespace std;


//*********************************************************************************
// Global variables
ifstream rfile,ifile;
int time_elapsed = 0;
enum state{CREATED,READY,RUNNG,BLOCK,PREEMPT};
struct process{
    int PID;
    int AT;
    int TC;
    int CB;
    int IO;
    int sprio;               // static priority
    int dprio;              // dynamic priority

    int cbremaining;
    int prev_state_time;    // time spent in the last state by the process
    int blocktime;          // time wasted in blocked state by the process
    int finishtime;         // time when the process finished
    int state_time_start;
    int cpu_wait_time;      // time spent in READY state
    state last_state;
    };

class event{
    public:
        int eventid;
        int timeline;
        process* process_;
        state oldstate;
        state newstate;
};

class compare{
public:
    bool operator() (event* e1,event* e2){
        if(e1->timeline==e2->timeline){
            return e1->eventid>e2->eventid;
        }
        return e1->timeline>e2->timeline;
    }
};

priority_queue<event*,vector<event*>, compare> EVQUEUE;

vector<process*> processes;
vector<process*> cprocesses;    // processes currently active
process* CURRENT_PROCESS = NULL;

const int inf_time = 20000;
int next_pid=0;
//int next_insertion=inf_time;
//int next_burst=inf_time;
int QUANTUM = inf_time;
int EVENTID = 0;

int IO_time=0;
int CPU_time=0;
int CPU_W_time=0;

bool descriptive = true;

class Scheduler{
            public:
            virtual void reduce_dynamic_priority(process*)=0;
            virtual void reset_dynamic_priority(process*)=0;
            virtual process* get_next_process()=0;
            virtual bool add_processes()=0;
            virtual void add_to_runqueue(process*)=0;
            };


//*********************************************************************************
// Initialize readers
void initialize_readers(){
    string junk;
    rfile.open("rfile");
    getline(rfile,junk);    // skipping the first line which holds the size
    ifile.open("input4");
}

// Send out a new random number each time it is called
int new_random_number(){
    if(rfile.eof()){
        string junk;
        rfile.seekg(0,ios::beg);
        getline(rfile,junk);
    }
    string rnum;
    getline(rfile,rnum);
    return atoi(rnum.c_str());
}

// Return a random number between the given burst and 1;
int get_new_burst(int burst){
    return 1 + (new_random_number() % burst);
}

// Read the file into memory for the first time
void get_processes(){
    string proc;
    while(!ifile.eof()){
        getline(ifile,proc);
        stringstream ss(proc);
        if(proc.size()){
            process* temp = new process();
            ss>>temp->AT>>temp->TC>>temp->CB>>temp->IO;
            temp->PID = processes.size();
            temp->sprio = get_new_burst(4);
            temp->dprio = temp->sprio-1;
            temp->cbremaining = 0;          // initializing with default values
            temp->prev_state_time = 0;      // ...
            temp->blocktime = 0;            // ...
            temp->finishtime = 0;           // ...
            temp->state_time_start = temp->AT;     // ...
            temp->cpu_wait_time = 0;        // ...
            temp->last_state = CREATED;
            processes.push_back(temp);
        }
    }
}

// Get start time of the next event from EVQUEUE
int get_next_event_time(){
    if(EVQUEUE.empty())
        return inf_time;
    return (EVQUEUE.top())->timeline;
}

// Get new processes in the current time range
bool get_current_processes(bool advance){
    cprocesses.clear();
    if(next_pid>=processes.size())
        return false;
    if(cprocesses.empty() && advance){                              // When RUNQUEUE is empty and it is absolutely nec. to advance timefron
        if(processes[next_pid]->AT<=get_next_event_time()){             // **** VERIFY, TODO
            time_elapsed = processes[next_pid]->AT;                     // **** VERIFY, uncommented: TODO
            cprocesses.push_back(processes[next_pid]);
            if(descriptive){
                cout<<time_elapsed<<" "<<processes[next_pid]->PID<<" "<<0
                <<": "<<"CREATED"<<" -> "<<"READY"<<endl;
            }
            next_pid++;
        }
    }
    while(next_pid<processes.size() && processes[next_pid]->AT<=time_elapsed){
        if(processes[next_pid]->AT<=get_next_event_time()){             // **** VERIFY, TODO
            cprocesses.push_back(processes[next_pid]);
            if(descriptive){
                cout<<time_elapsed<<" "<<processes[next_pid]->PID<<" "<<0
                <<": "<<"CREATED"<<" -> "<<"READY"<<endl;
            }
            next_pid++;
        }
    }
    return true;
}

// Create event in EVQUEUE
void put_event(process** proc, state oldstate, state newstate,int timeline){
    event* e = new event();
    e->eventid = EVENTID++;
    e->timeline = timeline;
    e->process_ = *proc;
    e->oldstate = oldstate;
    e->newstate = newstate;
    *proc = NULL;           // setting the CURRENT PROC to NULL
    EVQUEUE.push(e);
}

// Get the first event from the event queue
event* get_event(){
    int n = EVQUEUE.size();
    if(EVQUEUE.empty())
        return NULL;
    event* next_evt = EVQUEUE.top();
    time_elapsed = next_evt->timeline;  // **** VERIFY this TIME_ELAPSED change: TODO
    return EVQUEUE.top();
}

// Delete current event from EVQUEUE
void delete_event(){
    if(!EVQUEUE.empty()){
        event* ev = EVQUEUE.top();
        EVQUEUE.pop();
    }
}



// ROUND-ROBIN Scheduler
class RR:public Scheduler{
            queue<process*> RUNQUEUE;
            public:
            bool add_processes(){
                bool advance = RUNQUEUE.size()==0?true:false;   // advance the time_elapsed to next available proc if RUNQUEUE is empty
                if(get_current_processes(advance)){
                    for(int i=0;i<cprocesses.size();i++){
                        RUNQUEUE.push(cprocesses[i]);
                    }
                    return true;
                }
                return false;
            }

            void add_to_runqueue(process* proc){
                int n = RUNQUEUE.size();
                RUNQUEUE.push(proc);
                n=RUNQUEUE.size();
            }

            process* get_next_process(){
//                add_processes();                        // fetches all current processes
                int n = RUNQUEUE.size();
                if(n>0){
                    process* proc = RUNQUEUE.front();       // selects the correct process to be run (to be sent to the EVQUEUE later)
                    RUNQUEUE.pop();                         // deletes and adjusts the RUNQUEUE
                    return proc;                            // returns the process to be run (to be sent to the EVQUEUE later)
                }
                return NULL;
            }

            void reduce_dynamic_priority(process* proc){    // **** TODO commented out for testing purposes
//                if(proc->dprio==0)
//                    proc->dprio = proc->sprio-1;
//                else
//                    proc->dprio--;
            }

            void reset_dynamic_priority(process* proc){
                proc->dprio = proc->sprio-1;
            }
};

/*
    int cbremaining;
    int prev_state_time;
    int blocktime;          // IT => Time in BLOCK state per process
    int cpu_wait_time;      // CW => Time in READY state per process
    int finishtime;         // FT, TT
    int state_time_start;
    state last_state;

    int IO_time=0;          // Total time spent in the BLOCK STATE
    int CPU_time=0;         // Total time spent in the RUNNG STATE
    int CPU_W_time=0;       // IGNORE

    EVQUEUE, QUANTUM, CURRENT_PROCESS
*/
// Simulating actual process scheduling, execution, waiting and preemption
void simulate_processes(Scheduler* scheduler){
    event* e;
    bool call_scheduler=false;
    int cpu_block = 0;
    while((e = get_event())){
        state oldstate, nextstate;
        time_elapsed = e->timeline;
        oldstate = e->newstate;    // the next state will be the oldstate for the next event, if an event is created
        process* proc = e->process_;
        proc->last_state = oldstate;
        proc->prev_state_time = time_elapsed - proc->state_time_start;
        switch(oldstate){
            case READY:
//                scheduler->add_processes();     // **** VERIFY: TODO
                proc->state_time_start = time_elapsed;
                if(descriptive){
                    cout<<time_elapsed<<" "<<proc->PID<<" "<<proc->prev_state_time
                    <<": "<<"BLOCK"<<" -> "<<"READY"<<endl;
                }
                scheduler->add_to_runqueue(proc);
                if(e->oldstate==BLOCK){
                    scheduler->reset_dynamic_priority(proc);                // reseting dynamic priority
                }
                if(time_elapsed>cpu_block){                                  // time is beyond the range of a CPU block;
                    proc = NULL;                                                            // **** Whether to add a condition or not: TODO
                    call_scheduler = true;
                }
                break;

            case RUNNG:
                {
//                scheduler->add_processes();     // **** VERIFY: TODO
                int cb;
                proc->cpu_wait_time += proc->prev_state_time;
                if(proc->cbremaining>0){
                    cb = proc->cbremaining;
                    proc->cbremaining = 0;
                }else{
                    cb = get_new_burst(proc->CB);
                }
                int tc = proc->TC;
                cb = min(cb,tc);
                if(descriptive){
                    cout<<time_elapsed<<" "<<proc->PID<<" "<<proc->prev_state_time
                    <<": "<<"READY"<<" -> "<<"RUNNG"
                    <<" "<<"cb="<<cb<<" rem="<<proc->TC
                    <<" prio="<<proc->dprio<<endl;
                }
                // 1. Check if process termination is occurring: FINISHED STATE NEXT => remove from RUNQUEUE, CALL SCHEDULER
                if(tc<=cb && cb<=QUANTUM){
                    proc->state_time_start = time_elapsed;
                    time_elapsed += tc;
                    CPU_time += tc;
//                    if(descriptive){
//                        cout<<time_elapsed<<" "<<proc->PID<<" "<<cb         // **** CONFIRM "cb" :TODO
//                        <<": "<<"Done"<<endl;
//                    }
                    proc->TC -= tc;
                    proc->finishtime = time_elapsed;
                    put_event(&proc,oldstate,BLOCK,time_elapsed);
                    cpu_block = time_elapsed;
//                    proc = NULL;                        // **** So that the call_scheduler works: TODO
//                    call_scheduler=true;
//                    cpu_block = time_elapsed;           // to prohibit a READY process to be scheduled until CPU is active
                }
                // 2. Check if CPU burst ends before or at quantum: BLOCKING STATE NEXT
                else if(cb<=QUANTUM){
                    proc->state_time_start = time_elapsed;
                    time_elapsed += cb;
                    CPU_time += cb;

                    proc->TC -= cb;
                    put_event(&proc,oldstate,BLOCK,time_elapsed);       // added to EVQUEUE
                    cpu_block = time_elapsed;           // to prohibit a READY process to be scheduled until CPU is active
                }
                // 3. Check if CPU burst is longer than quantum: PREEMPTION STATE NEXT
                else if(cb>QUANTUM){
                    proc->state_time_start = time_elapsed;
                    time_elapsed += QUANTUM;
                    CPU_time += QUANTUM;

                    scheduler->reduce_dynamic_priority(proc);               // reducing dynamic priority after quantum expiration

                    proc->TC -= QUANTUM;
                    proc->cbremaining = cb-QUANTUM;
                    put_event(&proc,oldstate,PREEMPT,time_elapsed);     // added to EVQUEUE
                    cpu_block = time_elapsed;           // to prohibit a READY process to be scheduled until CPU is active
                }
                }
                break;

            case BLOCK:
                {
//                scheduler->add_processes();     // **** VERIFY: TODO
                if(proc->TC==0){
                    if(descriptive){
                        cout<<time_elapsed<<" "<<proc->PID<<" "<<proc->prev_state_time         // **** CONFIRM "cb" :TODO
                        <<": "<<"Done"<<endl;
                    }
                    proc = NULL;                        // **** So that the call_scheduler works: TODO
                    call_scheduler=true;
                }else{
                    proc->state_time_start = time_elapsed;
                    int io = get_new_burst(proc->IO);
                    if(descriptive){
                        cout<<time_elapsed<<" "<<proc->PID<<" "<<proc->prev_state_time
                        <<": "<<"RUNNG"<<" -> "<<"BLOCK "
                        <<" "<<"ib="<<io<<" rem="<<proc->TC<<endl;
                    }
                    proc->blocktime += io;
                    IO_time += io;

                    put_event(&proc,oldstate,READY,time_elapsed+io);
                    call_scheduler = true;
                }
                }
                break;

            case PREEMPT:
//                scheduler->add_processes();     // **** VERIFY: TODO
                proc->state_time_start = time_elapsed;
                if(descriptive){
                    cout<<time_elapsed<<" "<<proc->PID<<" "<<proc->prev_state_time
                    <<": "<<"RUNNG"<<" -> "<<"READY"
                    <<" "<<"cb="<<proc->cbremaining<<" rem="<<proc->TC
                    <<" prio="<<proc->dprio<<endl;
                }
                scheduler->add_to_runqueue(proc);
//                if(time_elapsed>cpu_block){                     // **** VERIFY: TODO check if this condition needs to be added
                    proc = NULL;
                    call_scheduler = true;
//                }
                break;
            default: exit(2);
        }
        delete_event();
        delete e;
        scheduler->add_processes();
        if(call_scheduler){
            if(get_next_event_time()==time_elapsed){
                continue;
            }
//            if(next_pid<processes.size() && get_next_event_time()<=processes[next_pid]->AT){   // **** VERIFY TODO
//                scheduler->add_processes();             // **** VERIFFY TODO
//                continue;
//            }
            call_scheduler=false;
            if(proc == NULL){
                proc = scheduler->get_next_process();
                if(proc == NULL)
                    continue;
            }
        int run_evt_time = max(time_elapsed,cpu_block);         // **** VERIFY: TODO
        put_event(&proc,proc->last_state,RUNNG,run_evt_time);   // **** VERIFY: TODO
        }
    }
}

// Main program (harness)
int main(){
    initialize_readers();
    get_processes();                                            // reads the file contents into a vector called processes
    Scheduler* scheduler = NULL;
    scheduler = new RR();
    QUANTUM = 2;
    // Initialization of processes and events prior to the scheduling
    scheduler->add_processes();                                 // **** VERIFY: TODO
    CURRENT_PROCESS = scheduler->get_next_process();            // the scheduler adds the processes in its run-queues, returns the selected process, modifies(removes from)
                                                                // run-queue with updated time_elapsed
    put_event(&CURRENT_PROCESS, CURRENT_PROCESS->last_state, RUNNG, time_elapsed);  // ANNULLS the CURRENT_PROCESS, and adds the process to the EVQUEUE
    simulate_processes(scheduler);                              // With at least one event in EVQUEUE, start the simulation of process execution
    return 0;
}
