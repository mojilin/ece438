#include <iostream>
#include <unordered_map>
#include <fstream>
#include <set>
#include <vector>

using namespace std;

class node {
    public:
    int backoff;
    int collisionNum;
    node (int s) {
        this->backoff = 0;
        this->collisionNum = 0;
    }
    void setRandom(vector<int> &R) {
        this->backoff = rand() % (R[this->collisionNum] + 1);
    }
};
// number of nodes N
// length of packet L
// initial random number R
// max retransmission attemp times M
// global time T
int N, L, R, M, T;
vector<int> Rvec;
ofstream fpOut("output.txt");


vector<int> findNodesToSend(int start, vector<node*> &nodeVec) {
    vector<int> res;
    for (int i = start; i  < nodeVec.size(); i++) {
        if (nodeVec[i]->backoff == 0) {
            res.push_back(i);
        }
    }
    return res;
}

void readFileAndInit(string filename) {
    ifstream in(filename);
    char c;
    string s;
    in >> c >> N >> c >> L >> c;

    while (in.get() != '\n') {
        in >> R;
        Rvec.push_back(R);
    }
    in >> c >> M >> c >> T;
}
double variance(vector<int> res) {
    double sum = 0;
    for (int i = 0; i  < res.size(); i++) {
        sum += res[i];
    }
    double mean = sum / res.size();
    double accum = 0;
    for (int i = 0; i < res.size(); i++) {
        accum += (res[i]-mean) * (res[i]-mean);
    }
    double var = accum / res.size();
    return var;
}
void simulate(int n) {

    vector<node*> nodeVec(n);
    for (int i = 0; i < n; i++) {
        nodeVec[i] = new node(Rvec[0]);
        nodeVec[i]->setRandom(Rvec);
    }
    // variables for statistics
    int curOccupyNode;
    int idleTime = 0;
    int totalCollisionNum = 0;
    int packetSentNum = 0;
    int endtime = 0;
    vector<int> successTransNum(n, 0);
    vector<int> collisions(n, 0);
    for (int time = 0; time < T; time++) {
        vector<int> nodesToSend = findNodesToSend(0, nodeVec);
        int sendNum = nodesToSend.size();

        if (sendNum == 0) {
            idleTime++;
            for (int i = 0; i < n; i++) {
                nodeVec[i]->backoff--;
            }
        } else if (sendNum > 1) {
            totalCollisionNum++;
            for (int i = 0; i < sendNum; i++) {
                nodeVec[nodesToSend[i]]->collisionNum++;
                collisions[nodesToSend[i]]++;
                if (nodeVec[nodesToSend[i]]->collisionNum == M) {
                    nodeVec[nodesToSend[i]]->collisionNum = 0;
                }
                nodeVec[nodesToSend[i]]->setRandom(Rvec);
            }
        } else {
            curOccupyNode = nodesToSend[0];
            if (time + L <= T) {
                time += L-1;
                packetSentNum++;
                successTransNum[curOccupyNode]++;
//                nodeVec[curOccupyNode]->collisionNum = 0;
                nodeVec[curOccupyNode]->setRandom(Rvec);
            } else {
                endtime = T - time;
                time = T ;
            }
        }
    }
//    fpOut << n <<" "<<(packetSentNum * L + endtime) * 1.0 / T << endl;
    fpOut << "Channel utilization " << (packetSentNum * L + endtime) * 1.0 / T<< endl;
    fpOut << "Channel idle fraction " << idleTime* 1.0/ T<< endl;
    fpOut << "Total number of collisions " << totalCollisionNum << endl;
    fpOut << "Variance in number of successful transmissions " << variance(successTransNum) << endl;
    fpOut << "Variance in number of collisions " << variance(collisions) << endl;

}
int main(int argc, char **argv) {

    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    readFileAndInit(argv[1]);

//    for (int l = 0; l < 5; l++) {
//        Rvec[0] = (int) pow(2, l);
//        for (int j = 1; j < Rvec.size(); j++) {
//            Rvec[j] = 2 * Rvec[j-1];
//            cout << Rvec[j] << " " << endl;
//        }
//        L = 20 * (l+1);
//        for (int i = 5; i <= 500; i++) {
//            simulate(i);
//        }
//    }

    simulate(N);
    fpOut.close();

    return 0;
}