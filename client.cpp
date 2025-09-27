/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name:
	UIN:    134004674
	Date:     09/25/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;


int main (int argc, char *argv[]) {


	// start server with fork()
	
	pid_t process_pid = fork();

	if(process_pid == -1){
		cerr << "fork failed" << endl;
		return 1;
	}

	// child process runs server
	if(process_pid == 0){
		// args[0], args
		// ex. char* args[] = {(char*)"ls", (char*)"-1", nullptr};
		char* args[] = {(char*)"./server", nullptr};
		execv(args[0], args);

		cerr << "exec didn't execute properly" << endl;
		return 1;
	}


	else{ // Do client stuff (parent process)

		// i think this stuff processes the command line args

		int opt;
		int p = -1;
		double t = -1;
		int e = -1;
		int m = MAX_MESSAGE;
		string filename = "";
		bool new_channel = false;

		vector<FIFORequestChannel*> channel_vector;

		while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
			switch (opt) {
				case 'p':
					p = atoi (optarg);
					break;
				case 't':
					t = atof (optarg);
					break;
				case 'e':
					e = atoi (optarg);
					break;
				case 'f':
					filename = optarg;
					break;
				case 'm':
					m = atoi(optarg);
					break;
				case 'c':
					new_channel = true;
					break;
			}
		}

		///////////////

		sleep(3); // make sure the server (child proc) is up first, else ugly stuff happens

		FIFORequestChannel control_chan("control", FIFORequestChannel::CLIENT_SIDE);
		channel_vector.push_back(&control_chan);

		// create new channel if   -c flag added
		if(new_channel){
			MESSAGE_TYPE newchan_msg = NEWCHANNEL_MSG;
			control_chan.cwrite(&newchan_msg, sizeof(MESSAGE_TYPE));

			char channel_name[30];
			control_chan.cread(&channel_name[0], sizeof(char)*30);
			//cout << "New channel name: " << channel_name << endl;

			FIFORequestChannel* new_chan = new FIFORequestChannel(channel_name, FIFORequestChannel::CLIENT_SIDE);
			channel_vector.push_back(new_chan);
		}

		FIFORequestChannel chan = *(channel_vector.back());

		// Data point request  (SINGLE DATA POINT)
		if(t != -1 && e != -1 && p != -1){
			char buf[sizeof(datamsg)]; // 256
			datamsg x(p, t, e);
			
			memcpy(buf, &x, sizeof(datamsg));
			chan.cwrite(buf, sizeof(datamsg)); // question
			double reply;
			chan.cread(&reply, sizeof(double)); //answer
			cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
		}


		// Save first 1000 lines into file x1.csv
		// This probably is 2000 calls of the first functionality  (bc ecg1 ecg2)
		else if(p != -1 && t == -1 && e == -1){
			// Format of file:
			// 59.996,-0.35,-0.51   times increase by +0.004  start at 0
			ofstream x1_file("received/x1.csv");
			if (!x1_file.is_open()) {
				cerr << "Couldn't open received/x1.csv for writing" << endl;
				return 1;
			}

			for (int line_i = 0; line_i < 1000; line_i++) {
				double current_time = line_i * 0.004;

				// ecg1 request
				datamsg request1(p, current_time, 1);
				char buf1[sizeof(datamsg)];
				memcpy(buf1, &request1, sizeof(datamsg));
				chan.cwrite(buf1, sizeof(datamsg));

				double reply1;
				chan.cread(&reply1, sizeof(double));

				// ecg2 request
				datamsg request2(p, current_time, 2);
				char buf2[sizeof(datamsg)];
				memcpy(buf2, &request2, sizeof(datamsg));
				chan.cwrite(buf2, sizeof(datamsg));

				double reply2;
				chan.cread(&reply2, sizeof(double));

				//write to file
				x1_file << current_time << "," << reply1 << "," << reply2 << "\n";
			}

			x1_file.close();
		}

		else if(filename != ""){ // -f flag for requesting whole file in /BIMDC
			// 1. send file message to get its length
			// 2. send a series of file messages to get content of file
			// 3. put received file under received/ with same name as orig
			// 4. create large file to transfer. check if identical with "diff"

			//auto start_time = chrono::high_resolution_clock::now();
			
			// getting file length
			filemsg fm(0, 0);
			string fname = filename;
			int len = sizeof(filemsg) + (fname.size() + 1);

			char* buf2 = new char[len];  //  probably heap allocated bc files can be huge (stack too small)
			memcpy(buf2, &fm, sizeof(filemsg));
			strcpy(buf2 + sizeof(filemsg), fname.c_str());
			chan.cwrite(buf2, len);  // I want the file length;

			int64_t file_len;
			chan.cread(&file_len, sizeof(int64_t));
			delete[] buf2;

			// getting content of file   
			
			int64_t curr_byte = 0;

			// create+open file to write into
			ofstream received_file(("received/"+fname), ios::binary); // we're copying raw bytes
			if (!received_file.is_open()) {
				cerr << "Couldn't open received/" << fname << endl;
				return 1;
			}

			while(curr_byte + m <= file_len){
				buf2 = new char[len];
				char* receive_buf = new char[m];
				filemsg file_msg(curr_byte, m);

				memcpy(buf2, &file_msg, sizeof(filemsg));
				strcpy(buf2 + sizeof(filemsg), fname.c_str());
				chan.cwrite(buf2, len);

				chan.cread(receive_buf, m);

				// write into file in received/"fname"
				received_file.write(receive_buf, m);

				curr_byte += m;
				delete[] buf2;
				delete[] receive_buf;
			}
			
			// check if there's still a few bytes left           testcase : file_len = 80   piece_len = 70   
			int64_t remaining_bytes = file_len - curr_byte;
			if(remaining_bytes > 0){
				buf2 = new char[len];
				char* receive_buf = new char[remaining_bytes];
				filemsg file_msg(curr_byte, remaining_bytes);

				memcpy(buf2, &file_msg, sizeof(filemsg));
				strcpy(buf2 + sizeof(filemsg), fname.c_str());
				chan.cwrite(buf2, len);

				chan.cread(receive_buf, remaining_bytes);

				// write into file in received/"fname"
				received_file.write(receive_buf, remaining_bytes);

				delete[] buf2;
				delete[] receive_buf;
			}

			received_file.close();

			// auto end_time = chrono::high_resolution_clock::now();
			// auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count();
			// cout << "Elapsed time (ms): " << elapsed_time << endl;
		}

		
		// closing the channel/s
		MESSAGE_TYPE quit_msg = QUIT_MSG;
		if(new_channel){
			chan.cwrite(&quit_msg, sizeof(MESSAGE_TYPE));
			delete channel_vector.back();
		}
		
		control_chan.cwrite(&quit_msg, sizeof(MESSAGE_TYPE));

		wait(nullptr);

	}
}
