#include "src/kv/KVClass.h"


unique_ptr<Server> global_srvr;

void terminating_system() {
    //frpintf(stdout, "Terminate the system!\n");
    global_srvr->Shutdown();
};


void sig_handler(int signo) {
    thread th_new(&terminating_system);
    th_new.join();
};


int main(int argc, char *argv[]) {

    int req_input_arg_size=6;

    // Check argument size
    if (argc==req_input_arg_size) {
        // OK
        fprintf(stdout,"Input size is OK!");
    }
    else {
        logf("<Error> Input format error!\n");
        exit(1);
    }

    string addr_m;

    int wrong_output=-1;
    
    vector<string> p_for_part;
    
    ifstream this_file;

    string this_l;

    int counterr=1;

    bool verbose1=false;

    string file_pth_default =argv[counterr];
    string log_path=argv[++counterr];
    string path_configuration=argv[++counterr];
    int partition_line_idx=stoi(argv[++counterr]);
    int ID_node=stoi(argv[++counterr]);

    bool check_part_l=(partition_line_idx>=1);
    if (check_part_l) {
        // OK
        if (verbose1){fprintf(stdout,"Partitions all OK!");};
    }
    else {
        logf("<Error> Partition ID wrong!\n");
        exit(1);
    }


    this_file.open(path_configuration,ios::in);
    bool check_open=(this_file.is_open());
    if (check_open) {
        if (verbose1){fprintf(stdout,"Status OK!");};
    }
    else {
        logf("<Error> Open file error!\n");
        exit(1);
    }
    if (getline(this_file,addr_m)) {
        //OK
        if (verbose1){fprintf(stdout,"Line 64 OK!");};
    }
    else {
        logf("<Error> Config file format  error! Missing master address!\n");
        exit(1);
    }

    int ii=1;
    for (; ii<=partition_line_idx-1; ++ii)
        getline(this_file,this_l);
    if (getline(this_file,this_l)) {
        //OK
        fprintf(stdout,"Partitions all OK!");
    }
    else {
        logf("<Error> Partition missing!\n");
        exit(1);
    }
    int x=0;
    int temp;
    int y=0;
    bool loop=true;
    while (loop) {
        if (y==string::npos){
            break;
        }
        y=this_l.find(',',x);
        int len;
        if (y==string::npos){
            len=y;
        }
        else {
            len=(y-x);
        }
        auto out_str=this_l.substr(x,len);
        p_for_part.push_back(out_str);
        temp=y;
        x=(++temp);
    }

    this_file.close();


    bool cond1=(ID_node<0);
    int ssizee=p_for_part.size();
    bool cond2=(ID_node<ssizee);
    bool cond_final=(cond2& (!cond1));
    if (!(cond_final)) {
        logf("<Error Node out of bound!\n");
        exit(1);
    }

    string addr_srv = p_for_part[ID_node];
    
    p_for_part.erase((p_for_part.begin()+ID_node));

    ServerBuilder svbd;

    signal(SIGINT,sig_handler);

    int id_part_l_=wrong_output+partition_line_idx;

    KVClass srvc = KVClass(p_for_part,id_part_l_, addr_srv,log_path,file_pth_default,addr_m);

    srvc.start_initialize();
    logf("<Process> Service initialzied!");

    svbd.AddListeningPort(addr_srv,InsecureServerCredentials());
    svbd.RegisterService(&srvc);

    logf("<Process> Server builder activated!");

    global_srvr = svbd.BuildAndStart();
    logf("<Process> Server activated!");

    global_srvr->Wait();

    logf("<Process> Server waiting to close!");

    srvc.close();
}