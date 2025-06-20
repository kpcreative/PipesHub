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
