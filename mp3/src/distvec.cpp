//
// Created by linmoji on 2019-11-15.
//

#include <iostream>
#include <unordered_map>
#include <fstream>
#include <set>
#include <vector>

using namespace std;

// topo
unordered_map<int, unordered_map<int, int>> topo;
// nodes
set<int> nodes;
// message
typedef struct message {
    int src;
    int dest;
    string msg;

    message(int src, int dest, string msg) : src(src), dest(dest), msg(msg) {}
} message;
vector<message> messageVec;

// filenames
string topofile, messagefile, changesfile;
// file
ofstream fpOut;
// forwaord table
unordered_map<int, unordered_map<int, pair<int, int> > > forward_table; // src, des, nexthop， cost

bool findItemInMap(int x, int y, unordered_map<int, unordered_map<int, int> > map) {
    auto it = map.find(x);
    auto res = it->second.find(y);
    return !(res == it->second.end());
}

void getTopo(string file, unordered_map<int, unordered_map<int, int>> &topo,
             unordered_map<int, unordered_map<int, pair<int, int>>> &forward_table) {
    ifstream topoin;
    topoin.open(file, ios::in);
    if (!topoin) {
        cout << "open file error!" << endl;
    }

    int src, dest, cost;
    while (topoin >> src >> dest >> cost) {
        topo[src][dest] = cost;
        topo[dest][src] = cost;
        if (nodes.find(src) == nodes.end()) {
            nodes.insert(src);
        }
        if (nodes.find(dest) == nodes.end()) {
            nodes.insert(dest);
        }
    }
    // init forward table
    for (auto k = nodes.begin(); k != nodes.end(); ++k) {
        src = *k;
        for (auto i = nodes.begin(); i != nodes.end(); ++i) {
            dest = *i;
            if (src == dest) {
                topo[src][dest] = 0;
            }
            if (!findItemInMap(src, dest, topo)) {
                topo[src][dest] = -999;
            }
            forward_table[src][dest] = make_pair(dest, topo[src][dest]);
        }
    }
    topoin.close();
}

void initForwardTable() {
    int src, dest;
    for (auto k = nodes.begin(); k != nodes.end(); ++k) {
        src = *k;
        for (auto i = nodes.begin(); i != nodes.end(); ++i) {
            dest = *i;
            forward_table[src][dest] = make_pair(dest, topo[src][dest]);
        }
    }
}

void getForwardTable(unordered_map<int, unordered_map<int, pair<int, int>>> &forward_table) {
    int num = nodes.size();
    int src, dest, min, minCost;
    for (int i = 0; i < num; ++i) {
        for (auto j = nodes.begin(); j != nodes.end(); ++j) {
            src = *j;
            for (auto k = nodes.begin(); k != nodes.end(); ++k) {
                dest = *k;
                min = forward_table[src][dest].first;
                minCost = forward_table[src][dest].second;
                for (auto m = nodes.begin(); m != nodes.end(); ++m) {
                    if (topo[src][*m] >= 0 && forward_table[*m][dest].second >= 0) {
                        int tempCost = topo[src][*m] + forward_table[*m][dest].second;
                        if (minCost < 0 || tempCost < minCost) {
                            min = *m;
                            minCost = tempCost;
                        }
                    }
                }
                forward_table[src][dest] = make_pair(min, minCost);
            }
        }
    }
    fpOut << endl;
    for (auto l = nodes.begin(); l != nodes.end(); ++l) {
        src = *l;
        for (auto i = nodes.begin(); i != nodes.end(); ++i) {
            dest = *i;
            fpOut << dest << " " << forward_table[src][dest].first << " " << forward_table[src][dest].second << endl;
        }
    }


}

void getMessage(string file) {
    ifstream msgfile(file);

    string line, msg;
    if (msgfile.is_open()) {
        while (getline(msgfile, line)) {
            int src, dest;
            sscanf(line.c_str(), "%d %d %*s", &src, &dest);
            msg = line.substr(line.find(" "));
            msg = msg.substr(line.find(" ") + 1);
            message newMessage(src, dest, msg);
            messageVec.push_back(newMessage);
        }
    }
    msgfile.close();
}

void sendMessage() {
    int src, dest, cost, nexthop;
    for (int i = 0; i < messageVec.size(); ++i) {
        src = messageVec[i].src;
        dest = messageVec[i].dest;
        nexthop = src;
        fpOut << "from " << src << " to " << dest << " cost ";
        cost = forward_table[src][dest].second;
        if (cost == -999) {
            fpOut << "infinite hops unreachable ";
        } else {
            fpOut << cost << " hops ";
            while (nexthop != dest) {
                fpOut << nexthop << " ";
                nexthop = forward_table[nexthop][dest].first;
            }
        }
        fpOut << "message " << messageVec[i].msg << endl;
    }
}

void doChanges(string file) {
    ifstream change(file);
    int src, dest, cost;
    if (change.is_open()) {
        while (change >> src >> dest >> cost) {
            topo[src][dest] = cost;
            topo[dest][src] = cost;
            initForwardTable();
            getForwardTable(forward_table);
            sendMessage();
        }
    }
}

int main(int argc, char **argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }
    topofile = argv[1];
    messagefile = argv[2];
    changesfile = argv[3];
    // get topo
    getTopo(topofile, topo, forward_table);
    fpOut.open("output.txt");
    // get forwarding table
    getForwardTable(forward_table);
    // print topo
    // get message
    getMessage(messagefile);
    // send message
    sendMessage();
    // do changes
    doChanges(changesfile);

    fpOut.close();
    return 0;
}