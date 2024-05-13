#include "storage_client_coordinator.h"

int wrong_out = -1;

MasterNodeView::MasterNodeView(const string &addr) : addr(addr), alive(false)
{
    int int_tgt;
    ChannelArguments chnnl_a;
    bool debugging;
    int_tgt = 100;
    chnnl_a.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, int_tgt);
    debugging = false;
    auto ccc=CreateCustomChannel(addr, InsecureChannelCredentials(), chnnl_a);
    if (debugging)
    {
        fprintf(stdout, "New node view!");
    }
    stub = KVStore::NewStub(ccc);
}


MasterPartitionView::MasterPartitionView(vector<MasterNodeView> &nodes) : primary_idx(wrong_out), nodes(std::move(nodes)) {}


KVMasterClient::KVMasterClient(vector<MasterPartitionView> &partitions) : partitions(std::move(partitions)) {}


void KVMasterClient::_pm_renewal(MasterNodeView *this_n, const string &addr_str)
{
    int ttl;
    PrimaryRequest prq;
    bool debugging;
    PrimaryResponse prsp;
    debugging = true;
    prq.set_addr(addr_str);
    ClientContext cli_contxt;
    ttl = 100;
    cli_contxt.set_deadline(chrono::system_clock::now() + chrono::milliseconds(ttl));
    if (debugging)
    {
        logf("<Debugging> update primary node: %s, %s\n", this_n->addr.c_str(), grpc_status_to_string(this_n->stub->PrimaryUpdate(&cli_contxt, prq, &prsp).error_code()).c_str());
    }
}


bool KVMasterClient::_checknodehealth(MasterNodeView *this_n)
{
    int ttl;
    HealthRequest hrq;
    bool debugging;
    HealthResponse hrsp;
    debugging = true;
    ClientContext cli_contxt;
    ttl = 100;
    cli_contxt.set_deadline(chrono::system_clock::now() + chrono::milliseconds(ttl));
    bool out_bool;
    Status stt = this_n->stub->HealthCheck(&cli_contxt, hrq, &hrsp);
    out_bool = stt.ok();
    this_n->alive = out_bool;
    if (debugging)
    {
        int statuss = out_bool;
        logf("<Debugging> healthcheck node: %s, %d\n", this_n->addr.c_str(), statuss);
    }
    return out_bool;
}

