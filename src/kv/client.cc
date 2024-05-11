#include "client.h"

string AA="round_";
string BB="robin";
string prefix_add;
int nilt=0;

Partition::Partition(vector<Node> &n):nodes(std::move(n)) {
    
    ChannelArguments chnl_a;
    bool debugging;
    
    string input_str=AA+BB;
    chnl_a.SetLoadBalancingPolicyName(input_str);

    
    int idx = 0;
    prefix_add="ipv4:";
    int count1=0,count2;
    for (; idx<-1+nodes.size(); idx++) {
        count1++;
        prefix_add=prefix_add+nodes[idx].addr;
        count2=0;
        if (idx<=nodes.size()-2) {
            count2++;
            prefix_add=prefix_add+",";
        }
    }
    auto chn=CreateCustomChannel(prefix_add,InsecureChannelCredentials(),chnl_a);
    debugging=true;
    stub=KVStore::NewStub(chn);
    if (debugging){
        logf("<Procress> Partition statrs over node!\n");
    }
    else {
        cout<<count1<<count2<<endl;
    }
        
}



KVClient::KVClient(const string &addr_m) {
    bool debugging=false;
    auto cc=CreateChannel(addr_m,InsecureChannelCredentials());
    if (debugging){
        cout<<addr_m<<endl;
    }
    master_stub = KVMaster::NewStub(cc);
    renew_part();
}



Partition &KVClient::row_partition(const string &rr) {
    int len_md5=16;
    uint8_t h8[len_md5];
    size_t ss=rr.size();
    bool debugging=false;
    MD5((const uint8_t*) rr.c_str(),ss,h8);
    int p_size=partitions.size();
    // For debugging
    if (debugging){
        cout<<"Partition size: "<<p_size<<endl;
    }
    int out_ID =(h8[nilt]%p_size);
    return partitions[out_ID];
}


