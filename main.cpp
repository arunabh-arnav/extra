#include<iostream>
#include<cstdlib>
#include<vector>
#include<queue>
#include<fstream>
#include<sstream>
#include<iomanip>
#include<stdio.h>
#include<stack>
#include<getopt.h>
#include<stdlib.h>
#include<ctype.h>
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
    int const_TC;

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

bool descriptive = false;

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
void initialize_readers(string randfile, string inpfile){
    string junk;
    rfile.open(randfile);
    getline(rfile,junk);    // skipping the first line which holds the size
    ifile.open(inpfile);
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
            temp->const_TC = temp->TC;
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
        else{
            break;
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

// Comparator for the SJF scheduler
class compare_jobs{
public:
    bool operator() (process* p1,process* p2){
        if(p1->TC==p2->TC){
            return p1->PID>p2->PID;
        }
        return p1->TC>p2->TC;
    }
};


// Shortest-Job-First Scheduler
class SJF:public Scheduler{
            priority_queue<process*,vector<process*>, compare_jobs> RUNQUEUE;
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
                    process* proc = RUNQUEUE.top();       // selects the correct process to be run (to be sent to the EVQUEUE later)
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

// Last-Come-First-Serve Scheduler
class LCFS:public Scheduler{
            stack<process*> RUNQUEUE;
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
                    process* proc = RUNQUEUE.top();       // selects the correct process to be run (to be sent to the EVQUEUE later)
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

// First-Come-First-Serve Scheduler
class FCFS:public Scheduler{
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

// Priority Scheduler
class PRIORITY:public Scheduler{
            queue<process*>* ACQUEUE;
            queue<process*>* EXQUEUE;
            void push_to_runqueue(process* proc){
                if(proc->dprio<0){              // gets added to EXQUEUE
                    proc->dprio = proc->sprio-1;    // first reset its dprio, then add
                    EXQUEUE[proc->dprio].push(proc);
                }else{                          // gets adde to ACQUEUE
                    ACQUEUE[proc->dprio].push(proc);
                }
            }
            int size_of_runqueue(){
                int n=0;
                for(int i=0;i<4;i++){
                    n += ACQUEUE[i].size()+EXQUEUE[i].size();
                }
                return n;
            }
            process* front_of_runqueue(){
                for(int i=3;i>=0;i--){
                    if(!ACQUEUE[i].empty())
                        return ACQUEUE[i].front();
                }
                queue<process*>* TEMPQUEUE = ACQUEUE;   // At this point it is important to swap the queues
                ACQUEUE = EXQUEUE;
                EXQUEUE = TEMPQUEUE;

                for(int i=3;i>=0;i--){
                    if(!ACQUEUE[i].empty())
                        return ACQUEUE[i].front();
                }
                return NULL;
            }
            void pop_from_runqueue(){
                for(int i=3;i>=0;i--){
                    if(!ACQUEUE[i].empty()){
                        ACQUEUE[i].pop();
                        return;
                    }
                }
                queue<process*>* TEMPQUEUE = ACQUEUE;   // At this point it is important to swap the queues
                ACQUEUE = EXQUEUE;
                EXQUEUE = TEMPQUEUE;

                for(int i=3;i>=0;i--){
                    if(!ACQUEUE[i].empty()){
                        ACQUEUE[i].pop();
                        return;
                    }
                }
                return;
            }
            public:
            PRIORITY(){
                ACQUEUE = new queue<process*>[4];
                EXQUEUE = new queue<process*>[4];
            }
            bool add_processes(){
//                bool advance = RUNQUEUE.size()==0?true:false;   // advance the time_elapsed to next available proc if RUNQUEUE is empty
                bool advance = size_of_runqueue()==0?true:false;
                if(get_current_processes(advance)){
                    for(int i=0;i<cprocesses.size();i++){
//                        RUNQUEUE.push(cprocesses[i]);
                          push_to_runqueue(cprocesses[i]);
                    }
                    return true;
                }
                return false;
            }

            void add_to_runqueue(process* proc){
//                int n = RUNQUEUE.size();
                  int n = size_of_runqueue();
//                RUNQUEUE.push(proc);
                  push_to_runqueue(proc);
//                n=RUNQUEUE.size();
                  n = size_of_runqueue();
            }

            process* get_next_process(){
//                add_processes();                        // fetches all current processes
//                int n = RUNQUEUE.size();
                int n = size_of_runqueue();
                if(n>0){
//                    process* proc = RUNQUEUE.front();       // selects the correct process to be run (to be sent to the EVQUEUE later)
                    process* proc = front_of_runqueue();
//                    RUNQUEUE.pop();                         // deletes and adjusts the RUNQUEUE
                    pop_from_runqueue();
                    return proc;                            // returns the process to be run (to be sent to the EVQUEUE later)
                }
                return NULL;
            }

            void reduce_dynamic_priority(process* proc){    // **** TODO commented out for testing purposes
                proc->dprio = proc->dprio-1;
//                if(proc->dprio<0)
//                    proc->dprio = proc->sprio-1;
//                else
//                    proc->dprio=proc->dprio-1;
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
    int io_block = 0;
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
//                scheduler->reduce_dynamic_priority(proc);               // reducing dynamic priority after quantum expiration
                if(descriptive){
                    cout<<time_elapsed<<" "<<proc->PID<<" "<<proc->prev_state_time
                    <<": "<<"BLOCK"<<" -> "<<"READY"<<endl;
                }
//                scheduler->add_to_runqueue(proc);
                if(e->oldstate==BLOCK){
                    scheduler->reset_dynamic_priority(proc);                // reseting dynamic priority
                }
                scheduler->add_to_runqueue(proc);
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
//                    proc->finishtime = time_elapsed;
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

//                    scheduler->reduce_dynamic_priority(proc);               // reducing dynamic priority after quantum expiration

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
                    proc->finishtime = time_elapsed;
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
//                    IO_time += io;

                    io_block = max(io_block,time_elapsed);
                    if(io_block<time_elapsed+io){           // **** VERIFY: TODO:
                        IO_time+=time_elapsed+io-io_block;
                        io_block = time_elapsed+io;
                    }

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
                scheduler->reduce_dynamic_priority(proc);
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

// Print summary of processes
void print_proc_summary(){
    for (int i = 0; i < processes.size(); i++){
        process* proc = processes[i];
        cout << setfill('0') << setw(4) << proc->PID << ": ";
        cout << setfill(' ') << setw(4) << proc->AT;
        cout << " " << setfill(' ') << setw(4) << proc->const_TC;
        cout << " " << setfill(' ') << setw(4) << proc->CB;
        cout << " " << setfill(' ') << setw(4) << proc->IO;
        cout << " " << proc->sprio;
        cout << " | ";
        cout << setfill(' ') << setw(5) << proc->finishtime << " ";
        cout << setfill(' ') << setw(5) << proc->finishtime - proc->AT << " ";
        cout << setfill(' ') << setw(5) << proc->blocktime << " ";
        cout << setfill(' ') << setw(5) << proc->cpu_wait_time;
        cout << endl;
    }
}

void print_simulation_summary(){
        double cpu_utilization, io_utilization, avg_turnaround, avg_wait, throughput;
        int total_turnaround = 0;
        int total_wait = 0;
        int last_finishtime=0;
        int total_block = 0;

        for (int i = 0; i < processes.size(); i++){
            process* proc = processes[i];
            last_finishtime = max(proc->finishtime,last_finishtime);
            cpu_utilization +=  proc->const_TC;
            total_turnaround += proc->finishtime - proc->AT;
            total_wait += proc->cpu_wait_time;
            total_block += proc->blocktime;
        }
        avg_turnaround = (double)total_turnaround/(double)processes.size();
        avg_wait = (double)total_wait/(double)processes.size();
        throughput = (double)processes.size()/((double)last_finishtime/100);

        cpu_utilization = ((double)CPU_time/last_finishtime)*100;
        io_utilization = ((double)IO_time/last_finishtime)*100;

//        printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",last_finishtime,cpu_utilization,io_utilization,
//                avg_turnaround,avg_wait,throughput);

        cout<<"SUM: "<<last_finishtime<<" "<<std::setprecision(2)<<std::fixed
            <<cpu_utilization<<" "<<io_utilization<<" "<<avg_turnaround<<" "
            <<avg_wait<<" "<<std::setprecision(3)<<std::fixed<<throughput<<endl;
}

// Main program (harness)
int main(int argc, char* argv[]){

    int commandLine;
	char sch_type = '#';
//	cin>>isVerbose;
	while((commandLine = getopt(argc, argv, "vs:"))!=-1){
		if (commandLine =='s'){
			sch_type = optarg[0];
			if (sch_type == 'R' || sch_type =='P')
				sscanf(optarg+1,"%d",&QUANTUM);
		}
		if (commandLine =='v')
			descriptive = true;
	}

    string ifile_name = argv[optind];
    string rfile_name = argv[optind+1];

    initialize_readers(rfile_name,ifile_name);
    get_processes();                                            // reads the file contents into a vector called processes
    Scheduler* scheduler = NULL; string sched_name;
    switch(sch_type){
	case 'F':
	{
		scheduler = new FCFS();
        QUANTUM = inf_time;
		sched_name = "FCFS";
		break;
	}
	case 'S':
	{
		scheduler = new SJF();
        QUANTUM = inf_time;
		sched_name = "SJF";
		break;
	}
	case 'L':
	{
		scheduler = new LCFS();
        QUANTUM = inf_time;
		sched_name = "LCFS";
		break;
	}
	case 'R':
	{
        stringstream ss;
		scheduler = new RR();
        ss<<"RR "<<QUANTUM;
        sched_name = ss.str();
		break;
	}
	case 'P':
	{
        stringstream ss;
		scheduler = new PRIORITY();
        ss<<"PRIO "<<QUANTUM;
        sched_name = ss.str();
		break;
	}
	default:
	{
		cout<<"Unknown Scheduler";
		return -1;
	}
	}
//    scheduler = new RR();
//    QUANTUM = 2;
    // Initialization of processes and events prior to the scheduling
    scheduler->add_processes();                                 // **** VERIFY: TODO
    CURRENT_PROCESS = scheduler->get_next_process();            // the scheduler adds the processes in its run-queues, returns the selected process, modifies(removes from)
                                                                // run-queue with updated time_elapsed
    put_event(&CURRENT_PROCESS, CURRENT_PROCESS->last_state, RUNNG, time_elapsed);  // ANNULLS the CURRENT_PROCESS, and adds the process to the EVQUEUE
    simulate_processes(scheduler);                              // With at least one event in EVQUEUE, start the simulation of process execution
//    cout<<"RR"<<QUANTUM<<"\n";
    cout<<sched_name<<endl;
    print_proc_summary();
    print_simulation_summary();
    return 0;
}
