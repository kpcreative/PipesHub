/*
--------------------------------------------------------------------------------
Author: Kartik Kumar Pandey
Language Used: C++
Title: Order Management System with Rate Limiting
--------------------------------------------------------------------------------

-----------------------------------------------------------------------------------
ASSUMPTIONS:--
-----------------------------------------------------------------------------------
- Order IDs are assumed to be unique startimg from 200.
- Orders will be given interactively using command-line input as i have done in this code according to it only.
- We assume system clock is accurate for trading hours check...you can even also input your starthr and endhr...and whenever you set your starthour at that time logon message will be sent same with loggedout .
- No actual exchange is integrated; "send" is mocked via print/log.
-also i have assumed this is running 24*7 so we have done this in while(true)...you will see in main
- i have also assumed that the response and all are provided by user dynamically
- i have also assumed that after each interval of 10sec we will be sending what ever the max_sending data we will set
- maxsendingdata=====i have writen as "MAXsendingDatapersecond"....so manage it
- i have assumed..(n--for new.   m-modify.   c-cancel.  r-response)
   so you can opt out for particular orderid what they want to see

- Response simulation is randomized (Accept/Reject)

- NOTE-----------------------------and after every 10 sec the fucntion i have made is outside but is inside the Class but not in our orderRequest function.
vvvvvvvvvvvvviiiii------------------------in orderreques() i have handled modifying canceling and putting into the queue..
                                          .but as soon as 10 seconds comes Threads run automatic locking the List and map and send all the max_data it can send
                                           from queue(list)


--- see the output log in order_log.txt



--------------------------------------------------------------------------------------------------------------------
Design Decisions:
---------------------------------------------------------------------------------------------------------------------
- Used a `list` and `unordered_map` combo for fast order access/modification,
  inspired by LRU cache patterns....as list will store all the data like queuing of data  and in map we can search fast during modify and Cancel and we can delete from list
   thats why i took map which will store order_id  and pointer to that order_id in the list

- used another map for storing sending request time of that particular id
  so during response recived it it will be in this map we can calculate the latency..

- Used `mutex` and `lock_guard` for thread-safe access to shared resources...Here shared resources is List and map...during sending after every 10 sec there should no be any order of same orderId modified or deleted


- Used a `thread` to flush (send) orders at fixed intervals (10 seconds)....this helps in automating all the response

- Logging is done to `order_log.txt` with latency measurement from send to response.

- Used `chrono::steady_clock` for precise latency measurement
- Separate thread flushes orders periodically to simulate real-time behavior
- Command-line interface enables manual testing of various order flows



---------------------------------------------------------------------------------------------------------

Architecture:------------
----------------------------------------------------------------------------------------------------------
- In orderManagement class -- i have made function send_request_data() it will be sending request after every 10sec...which i have called it from main
                              but this function will only get evoked when the resetSecondIfNeeded() is called because after each send orderrequest=0 na right?
                            thats why i make it 0 here in this fucntion then call the send_request_data.
                            Main class handling queueing, modifying, canceling, sending orders.

-`flushThread`: Background thread that processes queued orders based on the max rate...w

- `main()`: Handles input, generates random test data, and simulates user interaction.


overall--

1. Data Structures:
   - `list<OrderRequest>`: Maintains order queue (FIFO)
   - `unordered_map<orderId, iterator>`: Enables fast lookup/modification
   - `unordered_map<orderId, time_point>`: Tracks sent time for latency logging

2. Classes:
   - `OrderRequest` and `OrderResponse`: Store order data and response types
   - `OrderManagement`: Encapsulates core logic:
       ▪ Queue management
       ▪ Modify/Cancel handling
       ▪ Trading window check
       ▪ Thread-safe order flush(send_request) and logging

3. Threads:
   - `flushThread`: Periodically sends queued orders every 10 seconds
----------------------------------------------------------------------------------------------------------------
Tests:
----------------------------------------------------------------------------------------------------------------
Use `main()` to simulate:
- N → New Order
- M → Modify existing order (by ID) which you have given before 
- C → Cancel existing order (by ID) whcih you have given before
- R → Simulate response for sent order (logs latency)

Order dispatch occurs automatically every 10 seconds by a background thread.


---------------------------------------------------------------------------------------------------------------
 HOW TO COMPILE & RUN:
----------------------------------------------------------------------------------------------------------
 
First save the code..then follow these steps

first paste this in terminal----  g++ -std=c++17 code.cpp -o order_system
then this----------------------   ./order_system


*/

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <list>
#include <chrono>
#include <ctime>
#include <thread>
#include <random>
using namespace std;

// we used chrono for time tracking
using namespace chrono;



// we used this for proper locking mechanism...and we will use lock_guard..its a special type which dont nned to be unlocked it automatic gets unlocked
mutex dataMutex;

enum class RequestType
{
    Unknown = 0,
    New = 1,
    Modify = 2,
    Cancel = 3
};

enum class ResponseType
{
    Unknown = 0,
    Accept = 1,
    Reject = 2
};

struct OrderRequest
{
    int m_symbolId;
    double m_price;
    uint64_t m_qty;
    char m_side;
    uint64_t m_orderId;
    RequestType m_requestType;
};

struct OrderResponse
{
    uint64_t m_orderId;
    ResponseType m_responseType;
};

class OrderManagement
{
private:
    // we will set  it dynamkcally from main function
    int maxOrdersPerSecond;
    int startHour;
    int endHour;
    // used this boolean to check for login and logout at exactly endhour
    bool loggedon = false;

    // using doubly linked list so that we can easily modify the item as in map it will be easy for us to store the adress of list type ... just like LRU caching
    // we are using it so that if orderwill be in queue then it will be difficult for us to to search for it
    // replacing queue with-------->doubly linked list and map ..map storing id and adress of that particular thing in linked list
    list<OrderRequest> orderQueue;
    unordered_map<uint64_t, list<OrderRequest>::iterator> orderInQueueMap;
    unordered_map<uint64_t, steady_clock::time_point> orderSentTime;

    int ordersSentThisSecond = 0;

    // for writing into the log file we will create in that
    ofstream logFile;

public:
    OrderManagement(int maxOrdersPerSecond, int startHour, int endHour)
    {
        this->maxOrdersPerSecond = maxOrdersPerSecond;
        this->startHour = startHour;
        this->endHour = endHour;

        logFile.open("order_log.txt");
    }

    ~OrderManagement()
    {
        logFile.close();
    }

    // i used this for test case when i took total request when we want to send...but if we are servicing 24*7 then no needed
    bool isQueueEmpty()
    {
        lock_guard<mutex> lock(dataMutex);
        return orderQueue.empty();
    }
    // this function is needed as this is going to see and compare time is within timerange of order or not
    bool isWithinTradingWindow()
    {

        time_t now = time(0);
        tm *ltm = localtime(&now);

        if (ltm->tm_hour >= startHour && ltm->tm_hour < endHour)
        {
            if (!loggedon)
            {
                if (ltm->tm_hour == startHour)
                {
                    sendLogon();
                    loggedon = true;
                }
            }
            return true;
        }
        else
        {
            if (loggedon)
            {
                sendLogout();
                loggedon = false;
            }
            return false;
        }
    }

    void resetSecondIfNeeded()
    {
        ordersSentThisSecond = 0;
        send_request_data();
    }

    // this function is for when MaxorderperSecond we have not completed to send order as per required so what ever is in the queue we are flushing it and sending it
    // and we will call it using thread after evry particular second as given in question
    // thats why we used thread for it..and we locked it while sending the data no any input or response will be created
    // as if we dont use the lock then main thread may write in the List and this thread might also same time delete that...which is deadlock and racearound condition

    void send_request_data()
    {
        // this lock guard is special type where no need to give unlock as it automatically does
        lock_guard<mutex> lock(dataMutex);
        auto it = orderQueue.begin(); // give whatever is present at first side of linkedlist
        while (it != orderQueue.end() && ordersSentThisSecond < maxOrdersPerSecond)
        {
            OrderRequest req = *it;
            // map se v delete it
            orderInQueueMap.erase(req.m_orderId);
            // erased from list also
            it = orderQueue.erase(it);
            send(req);
            orderSentTime[req.m_orderId] = steady_clock::now();
            ordersSentThisSecond++;
        }
    }

    // this is sending request...if the request type is Modify and cancel and if it is in the List and map then delete or modify
    void onData(OrderRequest &&request)
    {
        // resetSecondIfNeeded();

        if (!isWithinTradingWindow())
        {
            cout << "[REJECT] Order outside trading window: " << request.m_orderId << endl;
            return;
        }
        lock_guard<mutex> lock(dataMutex);
        if (request.m_requestType == RequestType::Modify)
        {
            // thats why we have used map taki we can directly find which order needs to be modified
            auto it = orderInQueueMap.find(request.m_orderId);
            if (it != orderInQueueMap.end())
            {
                it->second->m_price = request.m_price;
                it->second->m_qty = request.m_qty;
                cout << "[MODIFY] Queued order updated: " << request.m_orderId << endl;
                return;
            }
        }

        if (request.m_requestType == RequestType::Cancel)
        {
            auto it = orderInQueueMap.find(request.m_orderId);
            if (it != orderInQueueMap.end())
            {
                orderQueue.erase(it->second);
                orderInQueueMap.erase(it);
                cout << "[CANCEL] Queued order removed: " << request.m_orderId << endl;
                return;
            }
        }
        else
        {

            orderQueue.push_back(request);
            auto it = --orderQueue.end();
            orderInQueueMap[request.m_orderId] = it;
            cout << "[QUEUE] Order queued: " << request.m_orderId << endl;
        }
    }

    // this part is for seeing and logging response where you can see it in order_log.txt...in my it has been generated
    void onData(OrderResponse &&response)
    {
        auto now = steady_clock::now();
        if (orderSentTime.find(response.m_orderId) != orderSentTime.end())
        {

            // thats why we used map for the storing time at which time it was sent from Middle
            auto sent = orderSentTime[response.m_orderId];
            auto latency = duration_cast<milliseconds>(now - sent).count();

            logFile << "OrderID: " << response.m_orderId
                    << " | Response: " << (response.m_responseType == ResponseType::Accept ? "Accept" : "Reject")
                    << " | Latency: " << latency << " ms" << endl;
        }
        else
        {
            logFile << "[WARN] Unknown orderId response: " << response.m_orderId << endl;
        }
    }

    void send(const OrderRequest &request)
    {

        cout << "[SEND] Order sent to exchange: " << request.m_orderId << endl;
    }

    void sendLogon()
    {
        cout << "[LOGON] Sent logon message to exchange" << endl;
    }

    void sendLogout()
    {
        cout << "[LOGOUT] Sent logout message to exchange" << endl;
    }
};

//------------Generating Test cases to see if it runs or not----------

// this I have made for the checking if my code is working fine or not
double getRandomPrice()
{
    static default_random_engine eng(random_device{}());
    uniform_real_distribution<double> dist(90.0, 110.0);
    return dist(eng);
}

uint64_t getRandomQty()
{
    static default_random_engine eng(random_device{}());
    uniform_int_distribution<uint64_t> dist(1, 100);
    return dist(eng);
}

char getRandomSide()
{
    return (rand() % 2 == 0) ? 'B' : 'S';
}
// --- Main Test Simulation ---
int main()
{
    int max_per_second, starthour, endhour, totalRequests;

    cout << "Enter the max orders per 10 second: ";
    cin >> max_per_second;
    cout << "Enter trading start hour: ";
    cin >> starthour;
    cout << "Enter trading end hour: ";
    cin >> endhour;

    // setting the how much we want to send per second and at what time you want to send message and start hpur for ordering...
    OrderManagement om(max_per_second, starthour, endhour);

    // this thread will get triggered after every 10s and whatever we have mentioned in maxdata we need to tranfer in this 10sec will be sent..
    thread flushThread([&om]()
                       {
    while (true) {
        this_thread::sleep_for(10s);
        om.resetSecondIfNeeded(); 
         // Exit condition: no more orders AND input loop is completed
        if (om.isQueueEmpty()) {
            break;
        }
    } });

    cout << "Giving 24*7 service" << endl;

    cout << "Use commands: N = New, M = Modify, C = Cancel, R = Response" << endl;

    uint64_t i = 100; // its just for test case as random order id we are creating
    while (true)
    {
        char cmd;

        {
            lock_guard<mutex> lock(dataMutex);
            cout << "\nRequest #" << " (N/M/C/R): ";
            cin >> cmd;
        }

        uint64_t orderId = 100 + i;
        if (cmd == 'N' || cmd == 'n')
        {

            double price = getRandomPrice();
            uint64_t qty = getRandomQty();
            char side = getRandomSide();
            om.onData(OrderRequest{1, price, qty, side, orderId, RequestType::New});
        }
        else if (cmd == 'M' || cmd == 'm')
        {
            uint64_t targetOrderId;
            {
                lock_guard<mutex> lock(dataMutex);
                cout << "Enter the orderId to modify : ";
                cin >> targetOrderId;
            }

            double price = getRandomPrice();
            uint64_t qty = getRandomQty();
            om.onData(OrderRequest{1, price, qty, 'B', targetOrderId, RequestType::Modify});
        }
        else if (cmd == 'C' || cmd == 'c')
        {
            uint64_t targetOrderId;
            {
                lock_guard<mutex> lock(dataMutex);
                cout << "Enter the orderId to cancel : ";
                cin >> targetOrderId;
            }
            om.onData(OrderRequest{1, 0, 0, 'B', targetOrderId, RequestType::Cancel});
        }
        else if (cmd == 'R' || cmd == 'r')
        {
            uint64_t targetOrderId;
            {
                lock_guard<mutex> lock(dataMutex);
                cout << "Enter the orderId to see response : ";
                cin >> targetOrderId;
            }
            ResponseType type = (rand() % 2 == 0) ? ResponseType::Accept : ResponseType::Reject;
            om.onData(OrderResponse{targetOrderId, type});
        }
        else
        {
            cout << "[SKIPPED] Invalid command. Use N/M/C/R only." << endl;
        }
        i++;
    }

    flushThread.join();

    return 0;
}
