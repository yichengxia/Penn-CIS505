#pragma once

#include <istream>
#include <openssl/md5.h>


#include <grpcpp/grpcpp.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include "src/protos/kv.grpc.pb.h"
#include "src/protos/kvmaster.grpc.pb.h"
#include "src/kv/err_except.h"
#include "src/common/common_util.h"

using namespace std;
using namespace grpc;


class Node
{
public:
    int node_from;
    bool alive;
    string node_ctnt;
    string addr;
    int nodeID=0;
    Node(const string &addr, bool alive) : addr(addr), alive(alive) {}
};

class Partition
{
public:
    string stub_ctnt;
    unique_ptr<KVStore::Stub> stub;
    int stub_count=0;
    vector<Node> nodes;
    int node_count=0;
    explicit Partition(vector<Node> &n);
};

class KVStorageClient
{

    vector<Partition> partitions;

    int counter=0;

    unique_ptr<KVMaster::Stub> master_stub;

    int wrong_output = -1;

    Partition &row_partition(const string &row);

    void renew_part()
    {
        Status stt;
        NodeView mrsp;
        bool debugging;
        NodeViewRequest mrq;
        debugging = true;
        ClientContext cli_contxt;
        
        stt=(master_stub->GetNodeView(&cli_contxt, mrq, &mrsp));
        bool cond = stt.ok();
        if (cond){
            // OK
            fprintf(stdout,"OK");
        }
        else {
            throw KVError(stt);
        }
            
        vector<Partition> vec_ps;
        if (debugging)
        {
            logf("<Debugging> Get new node view!\n");
        }
        int count1 = 0, count2;
        for (auto &pp : mrsp.partition())
        {
            count1++;
            count2 = 0;
            vector<Node> vec_n;
            for (auto &nn : pp.node())
            {
                count2++;
                vec_n.emplace_back(nn.address(), nn.alive());
            }
            if (!debugging)
            {
                cout << count2 << endl;
            }
            vec_ps.emplace_back(vec_n);
        }
        if (!debugging)
        {
            cout << count1 << endl;
        }
        partitions=std::move(vec_ps);
    }

public:

    explicit KVStorageClient(const string &addr_m);

    string get(const string &row, const string &col)
    {
        Status status;
        auto &tgt_r = row_partition(row);
        bool dbg;
        Status stt;
        int end_int = 4;
        int indexer = 0;
        int starter = 0;
        for (; indexer <= end_int; ++indexer)
        {
            ClientContext cli_txt;
            starter++;
            GetRequest grq;
            dbg = true;
            GetResponse grsp;
            
            grq.set_row(row);

            string output;
            grq.set_col(col);
            stt = tgt_r.stub->Get(&cli_txt, grq, &grsp);
            bool cond = (stt.ok());
            if (cond)
            {
                // OK
                if (dbg)
                {
                    logf("<Process> GET succeed!\n");
                }
            }
            else
            {
                bool cond2 = (StatusCode::NOT_FOUND != stt.error_code());
                if (!cond2)
                {
                    throw KVNotFound(stt);
                }
                continue;
            }
            output = grsp.data().value();
            return output;
        }
        throw KVError(stt);
    }

    int put(const string &row, const string &col, const string &data)
    {

        bool dbg;
        auto &tgt_r = row_partition(row);
        Status stt;
        int end_int = 4;
        int indexer = 0;
        int starter = 0;
        int out;
        for (; indexer <= end_int; ++indexer)
        {
            PutResponse prsp;
            ClientContext cli_txt;
            starter++;
            PutRequest prq;

            prq.set_row(row);
            prq.set_col(col);
            dbg = true;
            prq.set_value(data);
            bool cond_cont = false;
            stt = tgt_r.stub->Put(&cli_txt, prq, &prsp);
            cond_cont = (stt.ok());

            if (cond_cont)
            {
                // OK
                if (dbg == true)
                {
                    logf("<Process> PUT succeed!\n");
                }
            }
            else
            {
                continue;
            }
            out = prsp.bytes_written();
            return out;
        }
        throw KVError(stt);
    }


    string getdefault(const string &r, const string &c, const string &v)
    {
        string output;
        try
        {
            output = get(r, c);
            return output;
        }
        catch (KVNotFound &  )
        {
            output = v;
            return output;
        }
        return output;
    }

    int cput(const string &rr, const string &cc, const string &v1, const string &v2)
    {

        Status status;
        bool dbg;
        Status stt;
        int end_int = 4;
        int indexer = 0;
        int starter = 0;
        auto &xxx = row_partition(rr);
        for (; indexer <= end_int; ++indexer)
        {
            CPutRequest crq;
            CPutResponse crsp;
            starter++;
            ClientContext cli_txt;
            dbg = true;
            
            crq.set_row(rr);
            bool cond;
            crq.set_col(cc);

            int out;
            crq.set_v1(v1);
            crq.set_v2(v2);
            stt = xxx.stub->CPut(&cli_txt, crq, &crsp);

            cond = (stt.ok());
            if (cond)
            {
                if (dbg)
                {
                    logf("<Process> CPUT succeed!\n");
                }
            }
            else
            {
                bool case1, case2;
                case2 = (StatusCode::NOT_FOUND == status.error_code());
                case1 = (StatusCode::FAILED_PRECONDITION == status.error_code());
                if (case1)
                {
                    throw KVNoMatch(stt);
                }
                else if (case2)
                {
                    throw KVNotFound(stt);
                }
                continue;
            }
            out = crsp.bytes_written();
            return out;
        }
        throw KVError(stt);
    }

    int del(const string &rr, const string &cc)
    {
        Status status;
        bool dbg;
        Status stt;
        int end_int = 4;
        int indexer = 0;
        int starter = 0;
        auto &xxx = row_partition(rr);
        for (; indexer <= end_int; ++indexer)
        {
            DeleteResponse drsp;
            dbg = true;
            starter++;
            ClientContext ctx;
            DeleteRequest drq;
            bool cond;
            drq.set_row(rr);
            int out;
            drq.set_col(cc);
            stt = xxx.stub->Delete(&ctx, drq, &drsp);
            cond = (stt.ok());
            if (cond)
            {
                logf("<Process> DELETE succeed!\n");
            }
            else
            {
                continue;
            }
            out = drsp.keys_deleted();
            return out;
        }
        throw KVError(stt);
    }
};
