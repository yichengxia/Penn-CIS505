#include "src/kv_coordinator.h"


void signal_hanlder(int signal_fd) {
    thread this_th(&terminate_system);
    this_th.join();
}

int main(int argc, char *argv[]) {
    
    int req_arg_nr=2;

    vector<MasterPartitionView> partitions;

    // Check input size
    if (argc==req_arg_nr) {
        // OK
    }
    else {
        fprintf(stderr, "Argument error!\n");
        exit(1);
    }
    string add_m;

    string pth_configurations = argv[1];

    ifstream input_file;
    input_file.open(pth_configurations,ios::in);
    bool chect_open=(input_file.is_open());

    if (chect_open) {
        //fprintf(stdout, "Open succeed!\n");
    }
    else {
        fprintf(stderr, "Open failed!\n");
        exit(1);
    }

    string this_l;

    if (getline(input_file, add_m)) {
        //fprintf(stdout, "First line ok!\n");
    }
    else {
        fprintf(stderr, "First line getting failure!\n");
        exit(1);
    }

    
    while (getline(input_file, this_l)) {
        
        int x=0;
        vector<MasterNodeView> vec_n;
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
            vec_n.emplace_back(out_str);
            temp=y;
            x=(++temp);
        }

        partitions.emplace_back(vec_n);
    }
    
    ServerBuilder svbd;

    input_file.close();

    signal(SIGINT,signal_hanlder);


    KVMasterImpl srvc=KVMasterImpl(add_m,partitions);

    //fprintf(stdout, "Server waiting!\n");

    svbd.AddListeningPort(add_m,InsecureServerCredentials());

    svbd.RegisterService(&srvc);

    this_srvr = svbd.BuildAndStart();

    thread p_th(healthcheck_each_second, &srvc);

    this_srvr->Wait();

    fprintf(stdout, "Server waiting!\n");

    status_run=false;

    p_th.join();

}
