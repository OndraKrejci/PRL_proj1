
#include <array>
#include <fstream> // ifstream
#include <iostream> // cout, cerr

#include <mpi.h>

constexpr std::size_t INPUT_SIZE = 8;
constexpr int REQUIRED_PROCS = 19;

constexpr int MPI_OEMS_TAG = 1;

constexpr bool OUTPUT_FORMAT = true;

enum ERROR_CODE{
	ERR_INPUT_FILE = 1,
	ERR_ARGUMENTS,
	ERR_COMMUNICATION
};

void err_exit(const MPI_Comm& comm, const ERROR_CODE code){
	MPI_Abort(comm, code);
}

void printError(const int ec){
	char estring[MPI_MAX_ERROR_STRING];
	int len;
	MPI_Error_string(ec, estring, &len);
	std::cerr << estring << std::endl;
}

std::string parse_args(int argc, char** argv){
	if(argc < 2){
		std::cerr << "Missing input file argument\n";
		err_exit(MPI_COMM_WORLD, ERR_ARGUMENTS);
	}

	return std::string(argv[1]);
}

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

void printNumbers(const std::array<unsigned char, INPUT_SIZE>& numbers, const bool formatOut = false){
	const char sep = formatOut ? '\n' : ' ';

	std::cout << static_cast<short>(numbers[0]);
	for(unsigned i = 1; i < INPUT_SIZE; i++){
		std::cout << sep << static_cast<short>(numbers[i]);
	}
	std::cout << '\n';
}

int getCommRank(const MPI_Comm& comm){
	int commRank;
	MPI_Comm_rank(comm, &commRank);
	return commRank;
}

int getCommSize(const MPI_Comm& comm){
	int commSize;
	MPI_Comm_size(comm, &commSize);
	return commSize;
}

void sendNumbers(const unsigned char* const buf, const int dest, const MPI_Comm& comm, const int count){
	int ec = MPI_Send(buf, count, MPI_UNSIGNED_CHAR, dest, MPI_OEMS_TAG, comm);
	if(ec){
		std::cerr << "MPI_Send: failed to send numbers [" << getCommRank(comm) << " to " << dest << "]\n";
		printError(ec);
		err_exit(comm, ERR_COMMUNICATION);
	}
}

void recvNumbers(unsigned char* const buf, const int src, const MPI_Comm& comm, const int count){
	MPI_Status status;
	const int ec = MPI_Recv(buf, count, MPI_UNSIGNED_CHAR, src, MPI_OEMS_TAG, comm, &status);
	if(ec){
		std::cerr << "MPI_Recv: failed to receive numbers [" << getCommRank(comm) << " from " << src << "]\n";
		printError(ec);
		err_exit(comm, ERR_COMMUNICATION);
	}
}

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

void rootRecvNumbers(const std::array<int, INPUT_SIZE>& srcs, std::array<unsigned char, INPUT_SIZE>& numbers, const MPI_Comm& comm){
	MPI_Request reqs[INPUT_SIZE];
	for(unsigned i = 0; i < INPUT_SIZE; i++){
		const int ec = MPI_Irecv(&numbers[i], 1, MPI_UNSIGNED_CHAR, srcs[i], MPI_OEMS_TAG, comm, &reqs[i]);
		if(ec){
			std::cerr << "MPI_Recv: failed to receive numbers [" << getCommRank(comm) << " from " << srcs[i] << "]\n";
			printError(ec);
			err_exit(comm, ERR_COMMUNICATION);
		}
	}

	const int ec = MPI_Waitall(INPUT_SIZE, reqs, MPI_STATUSES_IGNORE);
	if(ec){
		std::cerr << "MPI_Waitall: failed to receive all numbers in root\n";
		printError(ec);
		err_exit(comm, ERR_COMMUNICATION);
	}
}

void compare(unsigned char* const buf){
	if(buf[0] < buf[1]){
		const unsigned char temp = buf[0];
		buf[0] = buf[1];
		buf[1] = temp;
	}
}

void net1x1(const int inH, const int inL, const int outH, const int outL, unsigned char* const buf, const MPI_Comm& comm){
	if(inH == inL){
		recvNumbers(buf, inH, comm, 2);
	}
	else{
		recvNumbers(buf, inH, comm, 1);
		recvNumbers(&buf[1], inL, comm, 1);
	}

	compare(buf);
	sendNumbers(buf, outH, comm, 1);
	sendNumbers(&buf[1], outL, comm, 1);
}

void oems(unsigned char* const buf, const MPI_Comm& comm){
	const int commRank = getCommRank(comm);

	if(commRank == 1) net1x1(0, 0, 4, 5, buf, comm);
	else if(commRank == 2) net1x1(0, 0, 6, 7, buf, comm);
	else if(commRank == 3) net1x1(0, 0, 6, 7, buf, comm);
	else if(commRank == 4) net1x1(0, 1, 10, 8, buf, comm);
	else if(commRank == 5) net1x1(0, 1, 8, 13, buf, comm);
	else if(commRank == 6) net1x1(2, 3, 10, 9, buf, comm);
	else if(commRank == 7) net1x1(2, 3, 9, 13, buf, comm);
	else if(commRank == 8) net1x1(4, 5, 12, 11, buf, comm);
	else if(commRank == 9) net1x1(6, 7, 12, 11, buf, comm);
	else if(commRank == 10) net1x1(4, 6, 0, 14, buf, comm);
	else if(commRank == 11) net1x1(8, 9, 14, 18, buf, comm);
	else if(commRank == 12) net1x1(8, 9, 16, 15, buf, comm);
	else if(commRank == 13) net1x1(5, 7, 15, 0, buf, comm);
	else if(commRank == 14) net1x1(10, 11, 16, 17, buf, comm);
	else if(commRank == 15) net1x1(12, 13, 17, 18, buf, comm);
	else if(commRank == 16) net1x1(14, 12, 0, 0, buf, comm);
	else if(commRank == 17) net1x1(14, 15, 0, 0, buf, comm);
	else if(commRank == 18) net1x1(11, 15, 0, 0, buf, comm);
}

int main(int argc, char** argv){
	MPI_Init(&argc, &argv);

	MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

	unsigned char buf[2];

	if(getCommRank(MPI_COMM_WORLD) == 0){
		const int commSize = getCommSize(MPI_COMM_WORLD);
		if(commSize < REQUIRED_PROCS){
			std::cerr << "Invalid amount of processors " << commSize << " (required " << REQUIRED_PROCS << ")\n";
			err_exit(MPI_COMM_WORLD, ERR_ARGUMENTS);
		}

		const std::string fname = parse_args(argc, argv);
		std::array<unsigned char, INPUT_SIZE> numbers = load_numbers(fname);
		printNumbers(numbers);

		rootSendNumbers(numbers, buf, MPI_COMM_WORLD);

		compare(buf);
		sendNumbers(buf, 4, MPI_COMM_WORLD, 1);
		sendNumbers(&buf[1], 5, MPI_COMM_WORLD, 1);

		constexpr std::array<int, INPUT_SIZE> srcs = {10, 16, 16, 17, 17, 18, 18, 13};
		rootRecvNumbers(srcs, numbers, MPI_COMM_WORLD);

		printNumbers(numbers, OUTPUT_FORMAT);
	}
	else{
		oems(buf, MPI_COMM_WORLD);
	}

	MPI_Finalize();

	return 0;
}
