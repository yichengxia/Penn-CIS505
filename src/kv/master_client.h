#pragma once

#include <thread>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include "src/protos/kv.grpc.pb.h"
#include "src/common/utils.h"
#include "src/kv/exceptions.h"

using namespace grpc;
using namespace std;

class MasterNodeView
{
public:
    int node_from;
    string addr;
    string node_ctnt;
    bool alive;
    int nodeID = 0;
    unique_ptr<KVStore::Stub> stub;
    int node_count = 0;
    explicit MasterNodeView(const string &addr);
};

class MasterPartitionView
{
public:
    vector<MasterNodeView> nodes;
    int part_view_count = 0;
    int primary_idx;
    string view_cntn;
    explicit MasterPartitionView(vector<MasterNodeView> &nodes);
};

class KVMasterClient
{
protected:
    bool status_checking_health=false;
    static void _pm_renewal(MasterNodeView *node, const string &p_addr);
    static bool _checknodehealth(MasterNodeView *node);

public:
    int nr_client = 0;
    vector<MasterPartitionView> partitions;
    int nr_parts = 0;

    explicit KVMasterClient(vector<MasterPartitionView> &partitions);

    bool adminshutdown(const std::string &this_n)
    {

        Status stt;
        ShutdownRequest srq;
        bool debugging;
        ShutdownResponse srsp;
        debugging = false;
        ClientContext cli_contxt;
        int count1 = 0, count2;

        for (auto &ppp : partitions)
        {
            count1++;
            for (auto &this_p : ppp.nodes)
            {
                bool cond1 = (this_p.addr != this_n);
                if (!cond1)
                {
                    count2++;
                    stt = this_p.stub->AdminShutdown(&cli_contxt, srq, &srsp);
                    break;
                }
            }
            // For debugging
            if (debugging)
            {
                cout << count1 << count2 << endl;
            }
        }

        bool cond2 = (stt.ok());

        if (cond2){
            return true;
        }
        else {return false;}
    }

    bool adminstartup(const std::string &this_n)
    {
        Status stt;
        StartupRequest srq;
        bool debugging;
        StartupResponse srsp;
        debugging = false;
        ClientContext cli_contxt;
        int count1 = 0, count2;

        for (auto &ppp : partitions)
        {
            count1++;
            for (auto &this_p : ppp.nodes)
            {
                bool cond1 = (this_p.addr != this_n);
                if (!cond1)
                {
                    count2++;
                    stt = this_p.stub->AdminStartup(&cli_contxt, srq, &srsp);
                    break;
                }
            }
        }
        // For debugging
        if (debugging)
        {
            cout << count1 << count2 << endl;
        }

        bool cond2 = (stt.ok());

        if (cond2){
            return true;
        }
        else {return false;}
    }

    vector<bool> do_healthcheck()
    {
        bool debugging = true;
        vector<thread> vec_ths;
        int count1, count2;
        vector<bool> stt_n;
        count1 = 0;
        for (auto &ppp : partitions)
        {
            count1++;
            for (auto &this_n : ppp.nodes)
            {
                vec_ths.emplace_back(&KVMasterClient::_checknodehealth, &this_n);
                count2++;
                stt_n.emplace_back(this_n.alive);
                // For debugging
                if (debugging)
                {
                    string stt;
                    if (this_n.alive)
                    {
                        stt = "Alive";
                    }
                    else
                    {
                        stt = "Down";
                    }
                    std::cout << "Info of Node:"
                              << " address: " << &this_n << "; liveness status: " << stt << std::endl;
                }
            }
        }
        debugging = false;

        int count3;
        for (auto &this_th : vec_ths)
        {
            count3++;
            this_th.join();
        }
        // For debugging
        if (debugging)
        {
            cout << count1 << count2 << count3 << endl;
        }

        return stt_n;
    }

    vector<pair<string, string>> displayAdminList()
    {
        bool debugging;
        vector<pair<string, string>> vec_out;
        int count1 = 0, count2;

        for (auto &ppp : partitions)
        {
            count1++;
            for (auto &this_p : ppp.nodes)
            {

                ListResponse lrsp;
                Status stt;
                ListRequest lrq;
                count2++;
                ClientContext contxt;
                bool cond1;
                stt = this_p.stub->AdminList(&contxt, lrq, &lrsp);
                cond1 = (stt.ok());
                if (!cond1)
                {
                    string disp = "Found err msg: " + stt.error_message();
                    cout << disp << endl;
                }
                else
                {
                    int count3 = 0;
                    for (const auto &kp : lrsp.keys())
                    {
                        debugging = true;
                        const std::string &rr = kp.row();
                        count3++;
                        
                        const std::string &cc = kp.col();
                        vec_out.emplace_back(rr, cc);
                        if (debugging)
                        {
                            std::cout << "Row number: " << rr << ", Column  number: " << cc << std::endl;
                        }
                    }
                    if (false)
                    {
                        std::cout << count1 << count2 << count3 << endl;
                    }
                    break;
                }
            }
        }

        return vec_out;
    }
};
