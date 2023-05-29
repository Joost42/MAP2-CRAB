#include <Arduino.h>
#include <queue>
#include <vector>
#include <limits.h>

//int crabCount = 4;
//int oldCrabCount = 5;

struct Node{
    Node* parent;
    int pathCost;
    int cost;
    int workerID;
    int jobID;
    bool assigned[16];
  };

Node* newNode(int x, int y, bool assigned[], Node* parent);
void removeCrabs(Node* min);
  
Node* newNode(int x, int y, bool assigned[], Node* parent){
    Node* node = new Node;

    for (int j = 0; j < oldCrabCount; j++)
        node->assigned[j] = assigned[j];
    node->assigned[y] = true;

    node->parent = parent;
    node->workerID = x;
    node->jobID = y;

    return node;
}

int calculateCost(int costMatrix[16][16], int x, int y, bool assigned[])
{
    int cost = 0;
    bool available[crabCount] = {true};

    for (int i = x + 1; i < oldCrabCount; i++)
    {
        int min = INT_MAX, minIndex = -1;

        for (int j = 0; j < crabCount; j++)
        {
            if (!assigned[j] && available[j] && costMatrix[i][j] < min)
            {
                minIndex = j;
                min = costMatrix[i][j];
            }
        }

        cost += min;
        available[minIndex] = false;
    }

    return cost;
}

struct comp
{
    bool operator()(const Node* lhs, const Node* rhs) const
    {
        return lhs->cost > rhs->cost;
    }
};

void removeCrabs(Node* min){
    if (min->parent == NULL){
        return;
    }
    
    removeCrabs(min->parent);
    if (min->jobID>crabCount-1){
      ei_printf(" min-> workerID = %u\n", min->workerID);
          if(oldCentroids[min->workerID].xas>43){
          uint8_t oldCount = EEPROM.read(0);
          ei_printf("    crab crossed x = 43 mark COUNTED, crab xas: %u\n", oldCentroids[min->workerID].xas);
          EEPROM.write(0, oldCount + 1);
          EEPROM.commit();
          return;
          } 
          ei_printf("    crab did not cross x = 43 mark NOT COUNTED, crab xas: %u\n", oldCentroids[min->workerID].xas);   
    }
}

int findMinCost(int costMatrix[16][16])
{
    std::priority_queue<Node*, std::vector<Node*>, comp> pq;

    bool assigned[oldCrabCount] = {false};
    Node* root = newNode(-1, -1, assigned, NULL);
    root->pathCost = root->cost = 0;
    root->workerID = -1;

    pq.push(root);

    while (!pq.empty())
    {
        Node* min = pq.top();
        pq.pop();

        int i = min->workerID + 1;

        if (i == oldCrabCount)
        {
            removeCrabs(min);
            return min->cost;
        }

        for (int j = 0; j < oldCrabCount; j++)
        {
            if (!min->assigned[j])
            {
                Node* child = newNode(i, j, min->assigned, min);
                child->pathCost = min->pathCost + costMatrix[i][j];
                child->cost = child->pathCost + calculateCost(costMatrix, i, j, child->assigned);
                pq.push(child);
            }
        }
    }
}

//int main()
//{
//
//    int costMatrix[16][16] =
//    {
//        {900, 200, 700, 800},
//        {6, 4, 3, 7},
//        {8, 2, 5, 6},
//        {1, 2, 6, 8},
//        {2, 3, 4, 5}
//    };
//
//    int minCost = findMinCost(costMatrix);
//    Serial.print("Minimum Cost: ");
//    Serial.println(minCost);
//    return 0;
//}
