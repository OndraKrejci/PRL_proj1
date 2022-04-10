
/**
 * PRL - Project 1 - Odd-Even Merge Sort
 * @file oems.cpp
 * @author Ondřej Krejčí (xkrejc69)
 * @date 4/2022
 */

#include <array>
#include <map>
#include <fstream> // ifstream
#include <iostream> // cout, cerr

#include <mpi.h>

constexpr std::size_t INPUT_SIZE = 8;
constexpr int REQUIRED_PROCS = 19;
constexpr int ROOT_RECV_PROCS_COUNT = 5;

constexpr int MPI_OEMS_TAG = 1;

constexpr bool OUTPUT_FORMAT = true;

enum ERROR_CODE{
	ERR_INPUT_FILE = 1,
	ERR_ARGUMENTS,
	ERR_COMMUNICATION
};

// Abort the execution after an error has occures
void err_exit(const MPI_Comm& comm, const ERROR_CODE code){
	MPI_Abort(comm, code);
}

// Prints error for the MPI error code
void printError(const int ec){
	char estring[MPI_MAX_ERROR_STRING];
	int len;
	MPI_Error_string(ec, estring, &len);
	std::cerr << estring << std::endl;
}

// Loads 8 numbers (size 1 byte) from the input file "numbers"
std::array<unsigned char, INPUT_SIZE> load_numbers(const std::string& fname){
	std::ifstream inp{fname};

	if(!inp.good()){
		std::cerr << "Failed to open file: " << fname << '\n';
		err_exit(MPI_COMM_WORLD, ERR_INPUT_FILE);
	}

	std::string line;
	if(!std::getline(inp, line)){
		std::cerr << "Input file " << fname << " is empty\n";
		err_exit(MPI_COMM_WORLD, ERR_INPUT_FILE);
	}

	inp.close();

	const std::size_t inputSize = line.length();
	if(inputSize != INPUT_SIZE){
		std::cerr << "Input file contains invalid amount of numbers " << inputSize << " (expected: " << INPUT_SIZE << ")\n";
		err_exit(MPI_COMM_WORLD, ERR_INPUT_FILE);
	}

	std::array<unsigned char, INPUT_SIZE> numbers;
	for(unsigned i = 0; i < INPUT_SIZE; i++){
		numbers[i] = line[i];
	}

	return numbers;
}

// Prints numbers in the initial format or the output format (for the sorted result)
void printNumbers(const std::array<unsigned char, INPUT_SIZE>& numbers, const bool formatOut = false){
	const char sep = formatOut ? '\n' : ' ';

	std::cout << static_cast<short>(numbers[0]);
	for(unsigned i = 1; i < INPUT_SIZE; i++){
		std::cout << sep << static_cast<short>(numbers[i]);
	}
	std::cout << '\n';
}

// Retrieves the rank of the processor
int getCommRank(const MPI_Comm& comm){
	int commRank;
	MPI_Comm_rank(comm, &commRank);
	return commRank;
}

// Retrieves the size of a communicator
int getCommSize(const MPI_Comm& comm){
	int commSize;
	MPI_Comm_size(comm, &commSize);
	return commSize;
}

// Sends numbers to a processor
void sendNumbers(const unsigned char* const buf, const int dest, const MPI_Comm& comm, const int count){
	int ec = MPI_Send(buf, count, MPI_UNSIGNED_CHAR, dest, MPI_OEMS_TAG, comm);
	if(ec){
		std::cerr << "MPI_Send: failed to send numbers [" << getCommRank(comm) << " to " << dest << "]\n";
		printError(ec);
		err_exit(comm, ERR_COMMUNICATION);
	}
}

// Receives numbers from a processor
void recvNumbers(unsigned char* const buf, const int src, const MPI_Comm& comm, const int count){
	MPI_Status status;
	const int ec = MPI_Recv(buf, count, MPI_UNSIGNED_CHAR, src, MPI_OEMS_TAG, comm, &status);
	if(ec){
		std::cerr << "MPI_Recv: failed to receive numbers [" << getCommRank(comm) << " from " << src << "]\n";
		printError(ec);
		err_exit(comm, ERR_COMMUNICATION);
	}
}


// Sends numbers from root to the other processors in the first layer
void rootSendNumbers(const std::array<unsigned char, INPUT_SIZE>& numbers, unsigned char* const buf, const MPI_Comm& comm){
	constexpr int ROOT_INIT_SEND_COUNT = (INPUT_SIZE / 2) -1;
	MPI_Request reqs[ROOT_INIT_SEND_COUNT];

	for(unsigned i = 2; i < INPUT_SIZE; i += 2){
		buf[0] = numbers[i];
		buf[1] = numbers[i + 1];
		const int dst = i / 2;
		const int ec = MPI_Isend(buf, 2, MPI_UNSIGNED_CHAR, dst, MPI_OEMS_TAG, comm, &reqs[(i / 2) - 1]);
		if(ec){
			std::cerr << "MPI_Isend: failed to send numbers [" << getCommRank(comm) << " to " << dst << "]\n";
			printError(ec);
			err_exit(comm, ERR_COMMUNICATION);
		}
	}

	const int ec = MPI_Waitall(ROOT_INIT_SEND_COUNT, reqs, MPI_STATUSES_IGNORE);
	if(ec){
		std::cerr << "MPI_Waitall: failed to send all numbers in root\n";
		printError(ec);
		err_exit(comm, ERR_COMMUNICATION);
	}

	buf[0] = numbers[0];
	buf[1] = numbers[1];
}

// Receives output of the whole sorting net
void rootRecvNumbers(const std::array<int, ROOT_RECV_PROCS_COUNT>& srcs, std::array<unsigned char, INPUT_SIZE>& numbers, const MPI_Comm& comm){
	MPI_Request reqs[ROOT_RECV_PROCS_COUNT];
	int offset = 0;
	for(unsigned i = 0; i < ROOT_RECV_PROCS_COUNT; i++){
		const int count = (i == 0 || i == (ROOT_RECV_PROCS_COUNT - 1)) ? 1 : 2;
		const int ec = MPI_Irecv(&numbers[offset], count, MPI_UNSIGNED_CHAR, srcs[i], MPI_OEMS_TAG, comm, &reqs[i]);
		if(ec){
			std::cerr << "MPI_Recv: failed to receive numbers [" << getCommRank(comm) << " from " << srcs[i] << "]\n";
			printError(ec);
			err_exit(comm, ERR_COMMUNICATION);
		}

		offset += count;
	}

	const int ec = MPI_Waitall(ROOT_RECV_PROCS_COUNT, reqs, MPI_STATUSES_IGNORE);
	if(ec){
		std::cerr << "MPI_Waitall: failed to receive all numbers in root\n";
		printError(ec);
		err_exit(comm, ERR_COMMUNICATION);
	}
}

// Places the numbers in buffer in the order LOW, HIGH
void compare(unsigned char* const buf){
	if(buf[0] > buf[1]){
		const unsigned char temp = buf[0];
		buf[0] = buf[1];
		buf[1] = temp;
	}
}

// 1 x 1 net
// Receives 2 numbers, compares them and sends the result LOW and HIGH
void net1x1(const int inH, const int inL, const int outH, const int outL, unsigned char* const buf, const MPI_Comm& comm){
	if(inH == inL){
		recvNumbers(buf, inH, comm, 2);
	}
	else{
		recvNumbers(buf, inH, comm, 1);
		recvNumbers(&buf[1], inL, comm, 1);
	}

	compare(buf);

	if(outH == outL){
		sendNumbers(buf, outH, comm, 2);
	}
	else{
		sendNumbers(buf, outH, comm, 1);
		sendNumbers(&buf[1], outL, comm, 1);
	}
}

// Each processor implements a 1 x 1 net
// Selects processors to receive from and send to based on its rank
void oems(unsigned char* const buf, const MPI_Comm& comm){
	const int commRank = getCommRank(comm);

	// ranks of processors in the whole sorting net
	// rank: in low, in high, out low, out high
	const std::map<int, std::array<int, 4>> net{
		{1, {0, 0, 4, 5}},
		{2, {0, 0, 6, 7}},
		{3, {0, 0, 6, 7}},
		{4, {0, 1, 10, 8}},
		{5, {0, 1, 8, 13}},
		{6, {2, 3, 10, 9}},
		{7, {2, 3, 9, 13}},
		{8, {4, 5, 12, 11}},
		{9, {6, 7, 12, 11}},
		{10, {4, 6, 0, 14}},
		{11, {8, 9, 14, 18}},
		{12, {8, 9, 16, 15}},
		{13, {5, 7, 15, 0}},
		{14, {10, 11, 16, 17}},
		{15, {12, 13, 17, 18}},
		{16, {14, 12, 0, 0}},
		{17, {14, 15, 0, 0}},
		{18, {11, 15, 0, 0}}
	};

	if(net.find(commRank) != net.end()){
		std::array<int, 4> conns = net.at(commRank);
		net1x1(conns[0], conns[1], conns[2], conns[3], buf, comm);
	}
}

// Odd-even merge sort algorithm for 8 numbers
int main(int argc, char** argv){
	MPI_Init(&argc, &argv);

	MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

	unsigned char buf[2];

	if(getCommRank(MPI_COMM_WORLD) == 0){
		const int commSize = getCommSize(MPI_COMM_WORLD);
		if(commSize < REQUIRED_PROCS){
			std::cerr << "Invalid amount of processors " << commSize << " (required: " << REQUIRED_PROCS << ")\n";
			err_exit(MPI_COMM_WORLD, ERR_ARGUMENTS);
		}

		const std::string fname = "numbers";
		std::array<unsigned char, INPUT_SIZE> numbers = load_numbers(fname);
		printNumbers(numbers);

		rootSendNumbers(numbers, buf, MPI_COMM_WORLD);

		compare(buf);
		sendNumbers(buf, 4, MPI_COMM_WORLD, 1);
		sendNumbers(&buf[1], 5, MPI_COMM_WORLD, 1);

		constexpr std::array<int, ROOT_RECV_PROCS_COUNT> srcs{10, 16, 17, 18, 13};
		rootRecvNumbers(srcs, numbers, MPI_COMM_WORLD);

		printNumbers(numbers, OUTPUT_FORMAT);
	}
	else{
		oems(buf, MPI_COMM_WORLD);
	}

	MPI_Finalize();

	return 0;
}
