#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <fstream>
#include <csignal>
#include <iostream>
#include <map>

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <fmt/core.h>

#include "src/protos/kv.grpc.pb.h"
#include "src/protos/kvmaster.grpc.pb.h"
#include "src/common/utils.h"
#include "src/kv/exceptions.h"

using namespace std;
using namespace grpc;


/*
This are the KV status labels
*/
// #define M_STATUS -1 // for normal working status
// #define UK_STATUS -5 // for unknown status

int M_STATUS=-1;
int UK_STATUS=-5;

/*
This is the main class of KV (including all crucial operations)
*/
class KVClass final : public KVStore::Service {

    // Set up & define class-global variables

    int num_kv_st=9;

    string addr;

    int logfd;

    string log_path;

    string abs_path_dir;

    unique_ptr<KVMaster::Stub> master;

    thread thd_chckpt;

    bool status_run=true;

    const vector<string> peer_addrs;
    
    bool status_serv=true;

    vector<unique_ptr<KVStore::Stub>> peers;

    bool global_debugging=false;
    
    int primary_idx; 

    mutex lklk;

    mutex lglk;

    map<string, mutex> lk_r;

    Tablet tablet;

    string file_pth_default;

    unsigned partition_idx;
    

    /*
    Function to synchronize tablet from the peers
    */
    void peer_to_syn_tab(const unique_ptr<KVStore::Stub> &ppp) {

        bool debugging=false;

        ClientContext client_contxt;

        KeyValue key_val_pair;

        TabletSyncRequest client_req;

        const lock_guard<mutex> lock(lglk);

        unique_ptr<ClientReader<KeyValue>> reader(ppp->TabletSync(&client_contxt,client_req));
        
        while (reader->Read(&key_val_pair)) {
            if (true){
                // For debugging
                // if (debugging){cout<<"Reading in While!"<<endl;};
            }
            conduct_p(key_val_pair.row(),key_val_pair.col(),key_val_pair.value());
        }

        bool finished=(reader->Finish().ok());
        if (finished) {
            logf("<Process> Sync tablet from peer success!\n");
        } else {
            logf("<Warning> Sync from peer issue!\n");
        }

    }

    /*
    Function to open an existing file
    */
    int open_file(const char *ccc){
        return open(ccc, O_WRONLY|O_CREAT|O_TRUNC,0644);
    }

    /*
    Function to set up the fmt
    */
    string get_fmt_data(string a,string b,string c){
        return fmt::format("DATA {0} {1} {2}\n{3}\n{4}\n", a.size(), b.size(), c.size(), a, b);
    }


    /*
    Function to save data to the tablet
    */
    int tbl_saving() {
        

        const lock_guard<mutex> lock(lglk);
        int wrong_output=-1;

        // Open tablets
        bool debugging=false;
        int open_int;
        open_int=open_file(file_pth_default.c_str());
        bool opid_cond=(open_int>wrong_output);
        if (opid_cond) {
            // For debugging
            if (debugging){cout<<"Opening ok!"<<endl;};
        }
        else {
            logf("<Error> Open table failure!\n");
            return wrong_output;
        }   

        // For each element, write in
        for (const auto &[rk,rr]:tablet.rows()) {
            size_t len_wrttn;
            size_t len_wrttn_;
            for (const auto &[ck,ddt]:rr.cols()) {
                
                string data_all;
                bool check_all_written;
                data_all = get_fmt_data(rk,ck,ddt);
                size_t data_all_size=data_all.size();
                
                len_wrttn=write(open_int, data_all.c_str(), data_all_size);
                check_all_written=(len_wrttn==data_all_size);
                if (check_all_written) {
                    // For debugging
                    cout<<check_all_written<<endl;
                }
                else {
                    logf("<Error> Writing to tablet failed!\n");
                    return wrong_output;
                }
                size_t ddt_sz=ddt.size();
                len_wrttn_=write(open_int, ddt.c_str(), ddt_sz);
                
                bool check_all_written2=(len_wrttn_==ddt_sz);
                if (check_all_written2) {
                    // For debugging
                    cout<<check_all_written2<<endl;
                }
                else {
                    logf("<Error> Writing to tablet failed!\n");
                    return wrong_output;
                }

                // Further a new line  of data
                bool check_write_new=(write(open_int, "\n",1)==1);
                if (check_write_new) {
                    // For debugging
                    if (debugging){cout<<"Writing new line ok!"<<endl;};
                }
                else {
                    logf("<Error> Writing to tablet failed!\n");
                    return wrong_output;
                }
            }
        }

        // Empty the log info
        int id_truncated=ftruncate(logfd,0);
        bool cond_empty=(wrong_output==id_truncated);
        if (cond_empty) {
            logf("<Error> Empty log failed!\n");
        }
        logf("<Process> Saved tabliet with check point!\n");

        return 0;
    }


    /*
    Function to retrieve the peer ID from address 
    */
    int retrieve_idx_for_peer(const string &addr_peer_this) {
        int wrong_out=-1;
        int output;
        auto this_obj=find(peer_addrs.begin(),peer_addrs.end(),addr_peer_this);
        bool cond=(this_obj!=peer_addrs.end());
        if (!cond) {
            return wrong_out;
        }
        output=distance(peer_addrs.begin(),this_obj);
        return output;
    }

    /*
    Function to load data from tablet
    */
    void tbl_lding() {
        // start_initializeialize file
        ifstream file_this;
        file_this.open(file_pth_default, ios::in | ios::binary);
        bool cond_open_check=(file_this.is_open());
        bool debugging=false;
        if (cond_open_check) {
            // For debugging
            if (debugging){fprintf(stdout,"Opening file all OK!");};
        }
        else {
            logf("<Warning> Cannot open tablet file!\n");
            return;
        }
        int gap;
        string l_this;
        string data_="DATA";
        int nilt=0;
        while ( getline(file_this,l_this) ) {
            string this_int;
            // For debugging
            if (debugging){cout<<this_int<<" "<<file_pth_default<<endl;};
            this_int=get_token(l_this, nilt, &gap);
            bool check_cond2=(this_int!=data_);
            if (check_cond2) {
                logf("<Error> Tablet entry error!\n");
                exit(1);
            }
            else {
                int oo=1;
                int rb,cb,db,gap2;
                //string temp;
                gap2=1+gap;
                //temp=get_token(l_this,oo+gap,&gap);
                rb=stoi(get_token(l_this,oo+gap,&gap));
                gap2=1+gap;
                //temp=get_token(l_this,oo+gap,&gap);
                cb=stoi(get_token(l_this,oo+gap,&gap));
                gap2=1+gap;
                //temp=get_token(l_this,oo+gap,&gap);
                db=stoi(get_token(l_this,oo+gap,&gap));
                string A(rb,nilt);
                file_this.read(A.data(),rb);
                file_this.seekg(oo,ios_base::cur);
                string B(cb,nilt);
                file_this.read(B.data(),cb);
                file_this.seekg(oo,ios_base::cur);
                string D(db,nilt);
                file_this.read(D.data(),db);
                file_this.seekg(oo,ios_base::cur);
                conduct_p(A,B,D);
            }
        }
    }

    /*
    Function to update/renew the primary ID
    */
    void renew_idx_for_pm(const string &addr_pm_this) {
        bool check_addr_consist=(addr_pm_this==addr);
        int wrong_out=-1;
        if (check_addr_consist) {
            logf("<Process> Primary index update success!\n");
            primary_idx=M_STATUS;
            return;
        }
        int p_id;
        p_id=retrieve_idx_for_peer(addr_pm_this);
        bool cond2=(p_id!=wrong_out);
        if (cond2) {
            // For debugging
            fprintf(stdout,"Primary index update all OK!");
        }
        else {
            logf("<Process> Primary index update success!\n");
            primary_idx=UK_STATUS;
            return;
        }
        primary_idx=p_id;
    }


    /*
    Function for mutex lock
    */
    mutex& do_for_lcking(const string &input_r) {
        const lock_guard<mutex> lock(lklk);
        return lk_r[input_r]; 
    }


    /*
    Main function to do the log-related operations
    */
    int log_operation() {

        string l_this;
        int wrong_out=-1;
        ifstream file_this;

        const lock_guard<mutex> lock(lglk);
        
        file_this.open(log_path,ios::in|ios::binary);
        bool cond1=file_this.is_open();
        if (cond1) {
            // For debugging
            fprintf(stdout,"Opening file OK!");
        }
        else{
            logf("<Error> Opening file error!\n");
            return wrong_out;
        }

        int oo=1;
        
        int point=1;
        int output=0;
        int gap;
        string tkn;
        int nilt=0;
        while (  getline(file_this,l_this) )  {
            output+=1;
            tkn=get_token(l_this,nilt,&gap);
            string put_operation="PUT";
            bool ccp=(tkn==put_operation);
            string delete_operation="DELETE";
            bool ccd=(tkn==delete_operation);
            
            if (ccp) {

                int rb,cb,db,gap2;

                string temp;
                gap2=1+gap;
                //temp=get_token(l_this,1+gap,&gap);
                rb=stoi(get_token(l_this,1+gap,&gap));
                gap2=1+gap;
                //temp=get_token(l_this,1+gap,&gap);
                cb=stoi(get_token(l_this,1+gap,&gap));
                gap2=1+gap;
                //temp=get_token(l_this,1+gap,&gap);
                db=stoi(get_token(l_this,1+gap,&gap));

                string rk(rb,nilt);
                file_this.read(rk.data(),rb);
                file_this.seekg(point, ios_base::cur);
                string ck(cb,nilt);
                file_this.read(ck.data(),cb);
                file_this.seekg(point, ios_base::cur);
                string ddt(db,nilt);
                file_this.read(ddt.data(),db);
                file_this.seekg(point, ios_base::cur);
                conduct_p(rk,ck,ddt);
                logf("<Process> Append log operation: PUT\n");
                
            }
            else if (ccd) {
                int gap2;
                int rb,cb;
                string temp;
                gap2==1+gap;
                //temp=get_token(l_this,1+gap,&gap);
                rb=stoi(get_token(l_this,1+gap,&gap));
                gap2==1+gap;
                //temp=get_token(l_this,1+gap,&gap);
                cb=stoi(get_token(l_this,1+gap,&gap));

                string rk(rb,nilt);
                file_this.read(rk.data(),rb);
                file_this.seekg(point,ios_base::cur);
                string ck(cb,nilt);
                file_this.read(ck.data(),cb);
                file_this.seekg(point,ios_base::cur);
                conduct_d(rk,ck);
                logf("<Process> Append log operation: DELETE\n");
                file_this.seekg(oo,ios_base::cur);
            } 
            else {
                logf("<Warning> Invalid log entry!\n");
                output-=1;
            }
        }
        return output;
    }


    /*
    Function to delete the log
    */
    int del_log(const string &rk, const string &ck) {
        int wrong_out=-1;
        const lock_guard<mutex> lock(lglk);
        string str_this;
        str_this=fmt::format("DELETE {0} {1}\n{2}\n{3}\n",rk.size(),ck.size(),rk,ck);//get_fmt_del(rk,ck);
        size_t str_this_size=str_this.size();
        size_t len_wrttn=write(logfd,str_this.c_str(),str_this_size);
        bool check_all_written=(len_wrttn==str_this_size);
        if (!check_all_written) {
            logf("<Error> Fail to write into log!\n");
            return wrong_out;
        }
        else{
            return len_wrttn;
        }
    }

    /*
    Function to update/renew the primary node
    */
    GetPrimaryResponse pm_renewal() {

        GetPrimaryResponse pmres;
        bool debugging=false;
        GetPrimaryRequest pmreq;
        pmreq.set_partition_idx(partition_idx);
        ClientContext cli_contxt;
        auto iiit=master->GetPartitionPrimary(&cli_contxt,pmreq,&pmres);
        bool ok_check=(iiit.ok());
        if (ok_check) {
            // For debugging
            if (debugging){cout<<"OK"<<endl;};
        }
        else {
            logf("<Warning> Contacting with master about primary is failed!\n");
            throw KVError(iiit);
        }
        renew_idx_for_pm(pmres.addr());
        return pmres;
    }
    
    /*
    Function to recover the partition
    */
    int recover_partition() {
        bool check_cond=(primary_idx!=UK_STATUS);
        if (!check_cond){
            pm_renewal();
        }
        int out=(primary_idx!=UK_STATUS);
        return out;
    }

    /*
    Function to implement checkpointing
    */
    void do_checkpt() {
        bool loop=true;
        int sleep_interval=1;
        int time_lim=60;
        bool debugging=false;
        while (true) {
            int ticktock=0;
            tbl_saving();
            while (ticktock<=59) {
                sleep(sleep_interval);
                if (status_run) {
                    // For debugging
                    if (debugging){cout<<"status_run checkpoint!"<<endl;};
                }
                else {
                    return;
                }
                ticktock+=1;
            }
        }
    }

    string get_fmt_put(string a,string b,string c){
        return fmt::format("PUT {0} {1} {2} {3} {4}\n", a.size(), b.size(), c.size(), a, b);
    }


    /*
    Function to put into the tablet given row, column and value
    */
    unsigned conduct_p(const string &rk, const string &ck, const string &ddt) {
        unsigned out;
        auto this_r=(*tablet.mutable_rows())[rk];
        out=-1;
        (*this_r.mutable_cols())[ck]=ddt;
        out=ddt.size();
        (*tablet.mutable_rows())[rk]=this_r;
        out=ddt.size();
        return out;
    }

    /*
    Function to put the checkpoints in log
    */
    int checkpt_log_p(const string &rk, const string &ck, const string &ddt) {
        
        int wrong_out=-1;
        const lock_guard<mutex> lock(lglk);
        string put_str;
        int oo=1;
        put_str=get_fmt_put(rk,ck,ddt);
        size_t put_str_size=put_str.size();
        size_t len_wrttn = write(logfd,put_str.c_str(),put_str_size);
        bool check1=(len_wrttn==put_str_size);
        if (!check1) {
            logf("<Error> Fail to write1!\n");
            return wrong_out;
        }
        size_t ddt_size=ddt.size();
        size_t len_wrttn2=write(logfd, ddt.c_str(),ddt_size);
        bool check2=(len_wrttn2==ddt_size);
        if (!check2) {
            logf("<Error> Fail to write2!\n");
            return wrong_out;
        }
        // Create a new line to write
        int new_line=write(logfd,"\n",oo);
        bool check3=(new_line==oo);
        if (!check3) {
            logf("<Error> Fail to write into log!\n");
            return wrong_out;
        }
        int output=len_wrttn2;
        output+=len_wrttn;
        return (++output);
    }


    /*
    Function to conduct the delete operation
    */
    unsigned conduct_d(const string &rk, const string &ck) {
        bool cond1;
        bool cond2;
        int def_out=0;
        cond1=(tablet.rows().contains(rk));
        if (cond1){
            // Pass
        }
        else {
            return def_out;
        }
        auto this_r=tablet.rows().at(rk);
        cond2=(this_r.cols().contains(ck)); 
        if (cond2){
            // Pass
        }
        else {
            return def_out;
        } 
        this_r.mutable_cols()->erase(ck);
        (*tablet.mutable_rows())[rk]=this_r;
        return (++def_out);
    }

    /*
    Subroutine function for multicast PUT
    */
    static void _p_multi_subroutine(KVStore::Stub *this_node, Status *stt, PutResponse *resp, const string &rk, const string &ck,const string &ddt) {
        PutResponse rsp_;
        int put_ID;
        ClientContext ctx;
        put_ID=0;
        PutRequest req;
        bool debugging;
        
        req.set_row(rk);
        req.set_col(ck);
        
        debugging=false;

        req.set_value(ddt);
        *stt=this_node->PPut(&ctx, req, &rsp_);
        if (debugging){
            fprintf(stdout,"Put ID:%d\n",put_ID);
        }
        resp->CopyFrom(rsp_); 
    }

    /*
    Subroutine function for multicast DELETE
    */
    static void _d_multi_subroutine(KVStore::Stub *this_node,  Status *stt, DeleteResponse *rsp1, const string &rk, const string &ck) {
        DeleteResponse rsp_;
        int del_ID;
        ClientContext cli_contxt;
        del_ID=0;
        DeleteRequest dq;
        bool debugging;
        dq.set_row(rk);
        dq.set_col(ck);

        debugging=false;

        *stt=this_node->PDelete(&cli_contxt,dq,&rsp_);
        if (debugging){
            fprintf(stdout,"Deleted ID:%d\n",del_ID);
        }
        
        rsp1->CopyFrom(rsp_); 
    }

    /*
    Main function for multicast PUT
    */
    void multic_p(const string &rk, const string &ck, const string &ddt) {
        
        vector<PutResponse> resp_vec;
        int count1,count2;
        vector<Status> stat_vec;
        count1=0;
        vector<thread> th_vec;
        count2=0;
        bool debugging=false;
        for (auto &this_p: peers) {
            count1++;
            auto st = stat_vec.emplace_back();
            auto rsp = resp_vec.emplace_back();
            count2++;
            th_vec.emplace_back(KVClass::_p_multi_subroutine,this_p.get(), &st, &rsp, rk, ck, ddt);
        }
        if (debugging){
            cout<<count1<<endl;
        }
        int it;
        it=0;
        count2=0;
        int lim=th_vec.size();
        for (; it <=lim-1; it++) {
            auto &st_this = stat_vec[it];
            auto &th_this = th_vec[it];
            bool condd=st_this.ok();
            count2++;
            if (!condd) {
                logf("<Error> Multicasting failed!\n");
            }
            th_this.join();
        }
    }


    /*
    Main function for multicast DELETE
    */
    void multic_d(const string &rk, const string &ck) {

        vector<Status> stat_vec;
        int count1,count2;
        vector<DeleteResponse> resp_vec;
        count1=0;
        vector<thread> th_vec;
        count2=0;
        bool debugging=false;
        for (auto &this_p:peers) {
            count1++;
            auto st = stat_vec.emplace_back();
            auto rsp = resp_vec.emplace_back();
            count2++;
            th_vec.emplace_back(KVClass::_d_multi_subroutine,this_p.get(),&st,&rsp,rk,ck);
        }
        if (debugging){
            cout<<count1<<count2<<endl;
        }
        int it;
        int lim=th_vec.size();
        it=0;
        for (; it<=lim-1; it++) {
            auto &st_this = stat_vec[it];
            auto &th_this = th_vec[it];
            bool condd=st_this.ok();
            if (!condd) {
                logf("<Error> Multicasting fail!\n");
            }
            th_this.join();
        }
    }


// Public functions

public:

    /*
    KVClass
    */
    KVClass( const vector<string> &partition_peer_addrs, int partition_idx, const string &addr, const string &log_path, const string &file_pth_default,
                const string &master_addr)
            : peer_addrs(partition_peer_addrs), primary_idx(UK_STATUS), addr(addr), log_path(log_path), file_pth_default(file_pth_default),
              partition_idx(partition_idx)
            {
        
        bool debugging=false;
        
        auto chnl = CreateChannel(master_addr, InsecureChannelCredentials());
        if (debugging){
            // For debugging
            logf("<Process> KV Implement!\n");
        }
        master=KVMaster::NewStub(chnl);
        for (const auto & p_this: partition_peer_addrs) {
            debugging=false;
            auto chnl_this = CreateChannel(p_this, InsecureChannelCredentials());
            if (debugging){
                // For debugging
                fprintf(stdout,"channel impl");
            }
            peers.push_back(KVStore::NewStub(chnl_this));
        }
    }

    // Override operations  functions:

    /*
    Function: GET operation based on gRPC
    */
    Status Get(ServerContext *ctxt, const GetRequest *prq, GetResponse *prsp) override {
        bool check_status=(status_serv==true);
        bool debugging;
        if (check_status){
            // For debugging
            if (debugging){fprintf(stdout,"Operation OK!");};
        }
        else {
            return Status(StatusCode::UNAVAILABLE, "Operation failed!");
        }
        const auto &rk = prq->row();
        debugging=true;
        const auto &ck = prq->col();
        if (debugging){fprintf(stdout,"Obtained row and column!");};
        const lock_guard<mutex> lock(do_for_lcking(rk));

        bool check_r=(tablet.rows().contains(rk));
        if (check_r) {
            // For debugging
            if (debugging){fprintf(stdout,"Row OK!");};
        }
        else {
            return Status(StatusCode::NOT_FOUND, "Row non-existent!");
        }
        
        auto this_r = tablet.rows().at(rk);

        bool check_sync_status=(this_r.cols().contains(ck));
        if (check_sync_status) {
            // For debugging
            if (debugging){fprintf(stdout,"Column OK!");};
        }
        else {
            return Status(StatusCode::NOT_FOUND, "Column non-existent!");
        }
        auto vvv = this_r.cols().at(ck);
        debugging=false;
        prsp->mutable_data()->set_row(rk);
        if (debugging){cout<<"Setting row, column and value!"<<endl;};
        prsp->mutable_data()->set_col(ck);
        prsp->mutable_data()->set_value(vvv);
        if (debugging){cout<<"Row succeed!"<<endl;};

        return Status::OK;
    }

    /*
    Function: PUT operation based on gRPC
    */
    Status Put(ServerContext *ctxt, const PutRequest *prq, PutResponse *prsp) override {
        bool check_status=(status_serv==true);
        if (check_status){
            // For debugging
        }
        else {
            return Status(StatusCode::UNAVAILABLE, "Operation failed!");
        }

        ClientContext cli_contxt;

        auto &rk = prq->row();
        check_status=true;
        auto &ck = prq->col();

        auto &ddt = prq->value();

        bool check_c_cont=ck.empty();
        bool check_r_cont=rk.empty();
        bool cond=(check_r_cont||check_c_cont);

        bool debugging=true;

        if (!cond) {
            // For debugging
            if (debugging){fprintf(stdout,"Row and column OK!");};
        }
        else {
            return Status(StatusCode::INVALID_ARGUMENT, "Empty row and/or column!");
        }

        bool cond2=recover_partition();
        if (cond2){
            // For debugging
            if (debugging){fprintf(stdout,"Unconnected node OK!");};
        }
        else {
            return Status(StatusCode::UNAVAILABLE, "Unconnected node for parititon!");
        }

        bool cond3=(primary_idx!=M_STATUS);
        if (cond3) {
            // For debugging
            if (debugging){fprintf(stdout,"Primary OK!");};
        }
        else {
            const lock_guard<mutex> lock(do_for_lcking(rk));
            debugging=false;
            multic_p(rk,ck,ddt);
            if (debugging){fprintf(stdout,"Broadcast put!");};
            checkpt_log_p(rk,ck,ddt);
            unsigned size_wrttn = conduct_p(rk, ck, ddt);
            if (debugging){cout<<"Size written: "<<size_wrttn<<endl;};
            prsp->set_bytes_written(size_wrttn);
            
            return Status::OK;
        }
        auto &ppp=peers[primary_idx];
        if (debugging){cout<<"Put success!"<<endl;};
        return ppp->Put(&cli_contxt,*prq,prsp);
    }


    /*
    Function: DELETE operation based on gRPC
    */
    Status Delete(ServerContext *context, const DeleteRequest *prq, DeleteResponse *prsp) override {
        bool check_status=(status_serv==true);
        bool debugging=true;
        if (check_status){
            // For debugging
            if (debugging){fprintf(stdout,"Admin node OK!");};
        }
        else {
            return Status(StatusCode::UNAVAILABLE, "Admin has terminated the node!");
        }

        ClientContext cli_contxt;

        auto &rk = prq->row();
        check_status=true;
        auto &ck = prq->col();

        bool cond2=recover_partition();
        if (cond2){
            // For debugging
            if (debugging){fprintf(stdout,"Partitoion connection OK!");};
        }
        else {
            return Status(StatusCode::UNAVAILABLE, "Unconnected node for parititon!");
        }

        bool cond3=(primary_idx!=M_STATUS);
        if (cond3) {
            // For debugging
            if (debugging){fprintf(stdout,"Primary ptatus OK!");};
        }
        else {
            const lock_guard<mutex> lock(do_for_lcking(rk));
            debugging=false;
            multic_d(rk,ck);
            del_log(rk,ck);
            if (debugging){cout<<"Deleting row, column and value!"<<endl;};
            unsigned aaa=conduct_d(rk,ck);
            if (debugging){cout<<"Deletedsize:"<<aaa<<endl;};
            prsp->set_keys_deleted(aaa);
            return Status::OK;
        }
        if (debugging){cout<<"Deleting done!"<<endl;};
        return peers[primary_idx]->Delete(&cli_contxt, *prq, prsp);
    }



    /*
    Function: CPUT operation based on gRPC
    */
    Status CPut(ServerContext *context, const CPutRequest *prq, CPutResponse *prsp) override {
        bool check_status=(status_serv==true);
        bool debugging=true;
        if (check_status){
            // For debugging
            if (debugging){fprintf(stdout,"Admin OK!");};
        }
        else {
            return Status(StatusCode::UNAVAILABLE, "Admin has terminated the node!");
        }

        ClientContext cli_contxt;

        
        auto &rk = prq->row();
        check_status=true;

        auto &ck = prq->col();

        bool check_c_cont=ck.empty();
        bool check_r_cont=rk.empty();
        bool cond=(check_r_cont||check_c_cont);

        if (!cond) {
            // For debugging
            if (debugging){fprintf(stdout,"Row and column OK!");};
        }
        else {
            return Status(StatusCode::INVALID_ARGUMENT, "Empty row and/or column!");
        }

        bool cond2=recover_partition();
        if (cond2){
            // For debugging
            if (debugging){fprintf(stdout,"Unconnected node OK!");};
        }
        else {
            return Status(StatusCode::UNAVAILABLE, "Unconnected node for parititon!");
        }

        bool cond3=(primary_idx!=M_STATUS);
        if (cond3) {
            // For debugging
            if (debugging){fprintf(stdout,"Primary index OK!");};
        }
        else {
            

            auto &v1 = prq->v1();
            auto &v2 = prq->v2();

            const lock_guard<mutex> lock(do_for_lcking(rk));
            

            bool cond1=v1.empty();
            bool cond2=tablet.rows().contains(rk);
            bool cond=(cond1||cond2);

            if (cond){
                // For debugging
                if (debugging){fprintf(stdout,"Row OK!");};
            }
            else {
                return Status(StatusCode::NOT_FOUND, "Row non-existent!");
            }
                
            auto this_r = (*tablet.mutable_rows())[rk];
            cond1=v1.empty();
            cond2=this_r.cols().contains(ck);
            cond=(cond1||cond2);

            if (cond){
                // For debugging
                if (debugging){fprintf(stdout,"Column OK!");};
            }
            else {
                return Status(StatusCode::NOT_FOUND, "Column non-existent!");
            }

            bool check_cond1=(cond1 && (!cond2) );
            bool cond_temp=(this_r.cols().at(ck)==v1);
            bool check_cond2=(cond_temp&&cond2);

            bool check_all=(check_cond1 || check_cond2);

            if (check_all){
                // For debugging
                if (debugging){fprintf(stdout,"Value match OK!");};
            }
            else {
                return Status(StatusCode::FAILED_PRECONDITION, "Mismatch value!");
            }
            debugging=false;
            multic_p(rk,ck,v2);
            checkpt_log_p(rk,ck,v2);
            if (debugging){cout<<"Setting row, column and value done!"<<endl;};
            auto vvvv=conduct_p(rk,ck,v2);
            if (debugging){cout<<"Size written: "<< vvvv <<endl;};  
            prsp->set_bytes_written(vvvv);
            
            return Status::OK;
        }
        auto &pp=peers[primary_idx];
        if (debugging){cout<<"CPUT done!"<<endl;};  
        return pp->CPut(&cli_contxt, *prq, prsp);
    }

    /*
    Function to update the primary
    */
    Status PrimaryUpdate(ServerContext *context, const PrimaryRequest *req, PrimaryResponse *resp) override {
        bool check_status=(status_serv==true);
        if (!check_status){
            return Status(StatusCode::UNAVAILABLE, "Admin has terminated the node!");
        }
        auto& addr_this=req->addr();
        renew_idx_for_pm(addr_this);

        return Status::OK;
    }

    /*
    Function: PPUT operation based on gRPC
    */
    Status PPut(ServerContext *context, const PutRequest *req, PutResponse *resp) override {
        bool check_status=(status_serv==true);
        bool debugging=true;
        if (check_status){
            // For debugging
            if (debugging){fprintf(stdout,"Admin OK!");};
        }
        else {
            return Status(StatusCode::UNAVAILABLE, "Admin has terminated the node!");
        }

        const lock_guard<mutex> lock(do_for_lcking(req->row()));

        auto &rk = req->row();
        check_status=true;
        auto &ck = req->col();

        auto &ddt = req->value();
        if (debugging){cout<<"PPUT done!"<<endl;};

        checkpt_log_p(req->row(), req->col(), req->value());
        debugging=false;

        unsigned aaa=conduct_p(req->row(), req->col(), req->value());
        if (debugging){cout<<"PPUT size: "<<aaa<<endl;};
        resp->set_bytes_written(aaa);

        return Status::OK;

        if (debugging){
            //fprintf(stdout, "OK");
        }
    }

    /*
    Function: PDELETE operation based on gRPC
    */
    Status PDelete(ServerContext *context, const DeleteRequest *req, DeleteResponse *resp) override {
        bool check_status=(status_serv==true);
        bool debugging=true;
        if (check_status){
            // For debugging
            if (debugging){fprintf(stdout,"Status OK!");};
        }
        else {
            return Status(StatusCode::UNAVAILABLE, "Admin has terminated the node!");
        }
        debugging;
        const lock_guard<mutex> lock(do_for_lcking(req->row()));
        debugging=false;
        del_log(req->row(),req->col());
        // For debugging
        if (debugging){
            // cout<<req->row()<<end;
        }
        unsigned aaa=conduct_d(req->row(), req->col());
        if (debugging){cout<<"PDelete size: "<<aaa<<endl;};
        resp->set_keys_deleted(aaa);
        if (debugging){cout<<"PDelete done!"<<endl;};
        return Status::OK;
    }

    /*
    Function to conduct regular health checking
    */
    Status HealthCheck(ServerContext *context, const HealthRequest *req, HealthResponse *resp) override {
        bool checker=!status_serv;
        if (checker)
            return Status(StatusCode::UNAVAILABLE, "Admin has terminated the node!");
        return Status::OK;
    }


    /*
    Function to implement the tablet synchronization
    */
    Status TabletSync(ServerContext *context, const TabletSyncRequest *req, ServerWriter<KeyValue> *writer) override {
        bool check_status=(status_serv==true);
        bool debugging=true;
        if (check_status){
            // For debugging
            if (debugging){fprintf(stdout,"Status OK!");};
        }
        else {
            return Status(StatusCode::UNAVAILABLE, "Admin has terminated the node!");
        }

        const lock_guard<mutex> lock(lglk);
        debugging=false;
        int count1=0,count2,count3;
        for (const auto &[rk, this_r]:tablet.rows()) {
            count1++;
            count2=0;
            count3=0;
            // Iterate over the rows and columns
            for (const auto &[ck,ddt]:this_r.cols()) {
                auto v_with_k = KeyValue();
                count2++;
                if (check_status){
                    v_with_k.set_row(rk);
                    count3++;
                    v_with_k.set_col(ck);
                    v_with_k.set_value(ddt);
                }
                
                writer->Write(v_with_k);
            }
            if (debugging){
                cout<<count2<<endl;
            }
        }

        if (debugging){
            cout<<count1<<endl;
        }

        if (check_status){
            return Status::OK;
        }
        
    }


    int open_file_a(const char *ccc){
        return open(ccc, O_WRONLY|O_CREAT|O_APPEND, 0644);
    }

    /*
    Function to initialize all the preparations work
    */
    void start_initialize() {
        tbl_lding();
        log_operation();
        logfd = open_file_a(log_path.c_str());
        int wrong_out=-1;
        bool check_fd=(logfd>=0);
        bool debugging=true;
        if (check_fd) {
            // For debugging
            if (debugging){fprintf(stdout,"Status OK!");};
        }
        else {
            logf("<Error> Open file failure!\n");
            exit(1);
        }

        try {
            int p_id;
            auto pm_response = pm_renewal();
            p_id=wrong_out;
            for (auto &this_p: pm_response.nodes()) {
                bool cond1=(this_p.alive());
                bool cond2=(this_p.address()==addr);
                if ((!cond2)&&cond1) {
                    p_id=retrieve_idx_for_peer(this_p.address());
                    break;
                }
            }
            bool check_c;
            check_c=(p_id==wrong_out);
            if (check_c) {
                logf("<Warning> Peers unavailable!\n");
            } else {
                peer_to_syn_tab(peers[p_id]);
            }
        } catch (KVError &) {
            // Pass
        }
        thd_chckpt=thread(&KVClass::do_checkpt,this);
    }


    /*
    Function to close everything related to KV
    */
    void close() {
        status_run=false;
        int closerID=logfd;
        global_debugging=false;

        thd_chckpt.join();
        tbl_saving();

        logf("<Process> Tablet saved!\n");
        ::close(closerID);
    }

    // Admin-related functions:

    /*
    Function to shut down the admin
    */
    Status AdminShutdown(ServerContext *context, const ShutdownRequest *req, ShutdownResponse *resp) override {
        bool expected=false;
        status_serv=expected;
        return Status::OK;
    }

    /*
    Function to start the admin
    */
    Status AdminStartup(ServerContext *context, const StartupRequest *req, StartupResponse *resp) override {
        bool expected=true;
        status_serv=true;
        return Status::OK;
    }

    /*
    Function to deal with the admin list
    */
    Status AdminList(ServerContext *context, const ListRequest *req, ListResponse *resp) override {

        int out_out_fed=0;
        bool debugging=false;
        vector<pair<string, vector<string>>> all_rs;
        
        int out_out_fed2;
        for (const auto &[rk, this_r]:tablet.rows()) {
            out_out_fed++;
            auto &[_, cs_in_rs]=all_rs.emplace_back(rk,vector<string>());
            out_out_fed2=0;
            for (const auto &[ck,ddt]:this_r.cols()) {
                out_out_fed2++;
                cs_in_rs.push_back(ck);
            }
            out_out_fed=out_out_fed2-out_out_fed;
            sort(cs_in_rs.begin(),cs_in_rs.end());
        }
        int out_out_fed3=0;
        
        sort(all_rs.begin(),all_rs.end(),[](const pair<string, vector<string>> &X, 
        const pair<string,vector<string>> &Y) -> bool {return (Y.first>X.first);}  );
        
        int out_out_fed4;
        for (const auto &[rk, all_cs_in]: all_rs) {
            out_out_fed4=0;
            out_out_fed3++;
            for (const auto &ck:all_cs_in) {
                auto ppp_this = resp->add_keys();
                out_out_fed4++;
                ppp_this->set_row(rk);
                ppp_this->set_col(ck);
            }
            out_out_fed3=out_out_fed4-out_out_fed3;
        }
        // For debugging
        if (debugging){
            cout<<out_out_fed<<" "<<out_out_fed2<<" "<<out_out_fed3<<" "<<out_out_fed4<<endl;
        }
        return Status::OK;
    }

    
};


