#!/bin/bash
SOURCE_DIRECTORY="source"
GCC_OUTPUT="gcc_output.txt"
TOOLS_DIR="tools"
PORTFINDER_DIR="portfinder"
PORTFINDER_EXEC="port_finder"
SERVER1="socket_server1"
SERVER2="socket_server2"
CLIENT="socket_client"
TESTCLIENT="test_client"
TESTCLIENT_DIR="testclient"
TESTSERVER="test_server"
TESTSERVER_DIR="testserver"
SMALL_TEST_FILE="small_file.txt"
BIG_TEST_FILE="big_file.txt"
VALGRIND_OUTPUT="valgrind_output.txt"

# Command to avoid infinite loops
TIMEOUT="timeout"

# Maximum allowed running time of a standard client (to avoid infinite loops)
MAX_EXEC_TIME=30

#*******************************KILL PROCESSES**********************************
function killProcesses
{
	# Kill running servers
	for f in `ps -ef | grep $SERVER1 | awk '{print $2}'`; do kill -9 $f &> /dev/null; done
	for f in `ps -ef | grep $SERVER2 | awk '{print $2}'`; do kill -9 $f &> /dev/null; done
	for f in `ps -ef | grep $TESTSERVER | awk '{print $2}'`; do kill -9 $f &> /dev/null; done

	# Kill running clients
	for f in `ps -ef | grep $CLIENT | awk '{print $2}'`; do kill -9 $f 2>&1 &> /dev/null; done
	for f in `ps -ef | grep $TESTCLIENT | awk '{print $2}'`; do kill -9 $f 2>&1 &> /dev/null; done
}

#**********************************CLEANUP***************************************
function cleanup
{
	echo -n "Cleaning up..."
	killProcesses
	# Delete previously generated folders and files (if they exist)
	rm -r -f temp*
	rm -r -f client_temp_dir
	rm -r -f client_temp_dir_0
	#rm -f valgrind_output*
	rm -f $TOOLS_DIR/$PORTFINDER_EXEC
	rm -f $TOOLS_DIR/types.[hc]
	rm -f $SOURCE_DIRECTORY/$CLIENT
	rm -f $SOURCE_DIRECTORY/$SERVER1
	rm -f $SOURCE_DIRECTORY/$SERVER2
	rm -f $SOURCE_DIRECTORY/types.h
	rm -f $SOURCE_DIRECTORY/types.c
	echo -e "\t\t\tOk!"
}

#********************************INVOKING rcpgen******************************
function rpcGen
{
	cd $TOOLS_DIR
	
	echo -n "Invoking rpcgen -h...";
	rpcgen -h types.x -o types.h >> $GCC_OUTPUT 2>&1
	if [ ! -e types.h ] ; then
		echo -e "\tFail! \n\t[ERROR] rpcgen has failed!."
		echo -e "\tGCC log is available in $SOURCE_DIRECTORY/$GCC_OUTPUT"
	else
		echo -e "\t\tOk!"
	fi
	
	echo -n "Invoking rpcgen -c...";
	rpcgen -c types.x -o types.c >> $GCC_OUTPUT 2>&1
	if [ ! -e types.c ] ; then
		echo -e "\tFail! \n\t[ERROR] rpcgen has failed!."
		echo -e "\tGCC log is available in $SOURCE_DIRECTORY/$GCC_OUTPUT"
	else
		echo -e "\t\tOk!"
	fi
	
	cd ..
	
	cp -f $TOOLS_DIR/types.* $SOURCE_DIRECTORY/
}

#***************************COMPILING TESTING TOOLS******************************
function compileTools
{
	echo -n "Compiling testing tools..."

	gcc -g -DTRACE -Wno-format-security -o $TOOLS_DIR/$PORTFINDER_EXEC $TOOLS_DIR/$PORTFINDER_DIR/*.c $TOOLS_DIR/*.c 2> /dev/null

	if [ ! -e $TOOLS_DIR/$PORTFINDER_EXEC ] || [ ! -e $TOOLS_DIR/$SMALL_TEST_FILE ] || [ ! -e $TOOLS_DIR/$BIG_TEST_FILE ] ; then
		echo -e "\tFail!. \n[ERROR] Unable to compile testing tools. Test aborted!"
		exit -1
	fi
	
	if [ ! -e $TOOLS_DIR/$TESTSERVER ] || [ ! -e $TOOLS_DIR/$TESTCLIENT ] ; then
		echo -e "\tFail!. \n[ERROR] Unable to compile testing tools. Test aborted!"
		exit -1
	else
		echo -e "\tOk!"
	fi
}

#********************************COMPILING SOURCES******************************
function compileSource
{
	cd $SOURCE_DIRECTORY
	rm -f $GCC_OUTPUT

	echo -n "Test 0.1 (compiling server1)...";
	gcc -g -o $SERVER1 server1/*.c *.c -Iserver1 -lpthread -lm >> $GCC_OUTPUT 2>&1 
	if [ ! -e $SERVER1 ] ; then
		TEST_01_PASSED=false
		echo -e "\tFail! \n\t[ERROR] Unable to compile source code for server1."
		echo -e "\tGCC log is available in $SOURCE_DIRECTORY/$GCC_OUTPUT"
	else
		TEST_01_PASSED=true
		echo -e "\tOk!"
	fi

	echo -n "Test 0.2 (compiling client)...";
	gcc -g -o $CLIENT client/*.c *.c -Iclient -lpthread -lm >> $GCC_OUTPUT 2>&1 
	if [ ! -e $CLIENT ] ; then
		TEST_02_PASSED=false
		echo -e "\tFail! \n\t[ERROR] Unable to compile source code for the client."
		echo -e "\tGCC log is available in $SOURCE_DIRECTORY/$GCC_OUTPUT"
	else
		TEST_02_PASSED=true
		echo -e "\tOk!"
	fi

	echo -n "Test 0.3 (compiling server2)...";
	gcc -g -o $SERVER2 server2/*.c *.c -Iserver2 -lpthread -lm >> $GCC_OUTPUT 2>&1 
	if [ ! -e $SERVER2 ] ; then
		TEST_03_PASSED=false
		echo -e "\tFail! \n\t[ERROR] Unable to compile source code for server2."
		echo -e "\tGCC log is available in $SOURCE_DIRECTORY/$GCC_OUTPUT"
	else
		TEST_03_PASSED=true
		echo -e "\tOk!"
	fi

	cd ..
}

#************************************RUN SERVER*************************************
# First argument: the server to be run
# Return value: the listening port
function runServer
{
	local FREE_PORT=`./$PORTFINDER_EXEC`				# This will find a free port
	$1 $FREE_PORT $2 &> /dev/null &						# Launch the server
	ensureServerStarted $FREE_PORT
	echo $FREE_PORT	# Returning to the caller the port on which the server is listening
}

#
# first parameter -> port to be checked
#
function ensureServerStarted
{
	# Ensure the server has started
	for (( i=1; i<=5; i++ ))	# Maximum 5 tries
	do
		#fuser $1/tcp &> /dev/null
		rm -f temp 2>&1 &> /dev/null
		netstat -ln | grep "tcp" | grep ":$1 " > temp
		if [[ -s temp ]] ; then
			# Server started
			break
		else
			# Server non started
			sleep 1
		fi
	done
	rm -f temp 2>&1 &> /dev/null
}

#
# first parameter -> server to be used 
# second parameter -> client to be used
# third parameter -> file to be used
# fourth parameter -> returnCode (can be 1, 2, 3 or 4)
# fifth parameter -> test suite
# sixth parameter -> test number
#
function testClientServerInteraction
{
	# Creating temp directories
	mkdir temp_dir 2>&1 &> /dev/null
	mkdir client_temp_dir 2>&1 &> /dev/null
	
	# Preparing the server
	if [ "$1" == "socket_server1" ] || [ "$1" == "socket_server2" ] ; then
		cp -f $SOURCE_DIRECTORY/$1 temp_dir 2>&1 &> /dev/null
	else
		cp -f $TOOLS_DIR/$1 temp_dir 2>&1 &> /dev/null
	fi
	cp -f $TOOLS_DIR/$PORTFINDER_EXEC temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/$SMALL_TEST_FILE temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/$BIG_TEST_FILE temp_dir 2>&1 &> /dev/null
	
	testStudentClient=false
	
	# Preparing the client
	if [ "$2" == "socket_client" ] ; then
		cp -f $SOURCE_DIRECTORY/$2 client_temp_dir 2>&1 &> /dev/null
		testStudentClient=true
	else
		cp -f $TOOLS_DIR/$2 client_temp_dir 2>&1 &> /dev/null
	fi
	
	# Run the server
	echo -n "Running server $1..."
	pushd temp_dir >> /dev/null
		server_port=$(runServer "./$1")
		echo -e "\t[PORT $server_port] Ok!"
	popd >> /dev/null
	
	echo -n "[TEST $test_suite.$6] Launching $2 on port $server_port..."
	
	# Run testclient
	pushd client_temp_dir >> /dev/null
		$TIMEOUT $MAX_EXEC_TIME ./$2 "127.0.0.1" "$server_port" "$3" 2>&1 &> /dev/null
		rc=$?
		if (( $4 == 1 )) ; then
			return1=$rc
		elif (( $4 == 2 )) ; then
			return2=$rc
		elif (( $4 == 3 )) ; then
			return3=$rc
		elif (( $4 == 4 )) ; then
			return4=$rc
		else
			:
		fi
		echo -e -n "\tReturn code = $rc"
	popd >> /dev/null
	
	test=$(($6 + 1))
	if [[ $testStudentClient == false ]] && (( $rc != 3 )) ; then
		# We are using testclient in this test and the return code is different from 3 -> skip following tests
		echo -e "\t\t\t\t[--TEST $5.$6 FAILED--] $1 didn't respond correctly. Skipping test $5.$6/$5.$test"
		return
	fi
	
	if (( $rc == 3 )) ; then
		# Check the file existence
		if [ -e client_temp_dir/$3 ] ; then
			echo -e "\t\t\t\t[++TEST $5.$6 passed++] Ok!"
			
			if [ $5 == 2 ] && [ $6 == 1 ] ; then
				TEST21PASSED=true
			fi
			
			# Check received file is the same as the sample file
			echo -n "[TEST $5.$test] Checking received file is the same as the sample file..."
			diff "client_temp_dir/$3" "$TOOLS_DIR/$3" 2>&1 &> /dev/null
			rc=$?
			if [ $rc -eq 0 ] ; then
				echo -e "\t\t\t\t[++TEST $5.$test passed++] Ok!"
			else
				echo -e "\t\t\t\t[--TEST $5.$test FAILED--] Files are not equal!"
			fi
			
		else
			echo -e "\t\t[--TEST $5.$6 FAILED--] File is not present in the client directory. Skipping test $5.$test"
		fi
	else
		echo -e "\t\t\t\t[--TEST $5.$6 FAILED--] $1 didn't respond correctly or some problems occurred. Skipping test $5.$6/$5.$test"
	fi

	rm -r -f temp_dir 2>&1 &> /dev/null
	rm -r -f client_temp_dir 2>&1 &> /dev/null
	killProcesses
}

#
# first parameter -> server to be started (socket_server1 OR socket_server2)
# second parameter -> test_suite
# third parameter -> test number
#
function testValgrind
{
	# Creating temp directories
	mkdir temp_dir 2>&1 &> /dev/null
	mkdir client_temp_dir 2>&1 &> /dev/null
	
	# Preparing the server
	cp -f $SOURCE_DIRECTORY/$1 temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/$PORTFINDER_EXEC temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/$SMALL_TEST_FILE temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/$BIG_TEST_FILE temp_dir 2>&1 &> /dev/null
	
	# Preparing the testclient
	cp -f $TOOLS_DIR/$TESTCLIENT client_temp_dir 2>&1 &> /dev/null

	# Kill everything and restart for a new clean test
	killProcesses
	# Find a free port
	PORT=`$TOOLS_DIR/$PORTFINDER_EXEC`
	
	echo -n "Running student's server on port $PORT..."
	pushd temp_dir >> /dev/null
		null=`valgrind --leak-check=full -v --undef-value-errors=no --trace-children=yes --log-file=../%p_$VALGRIND_OUTPUT ./$1 "$PORT" &> /dev/null &`		# Launch the server
		#valgrind --leak-check=full --show-reachable=yes --trace-children=yes --log-file=../%p_$VALGRIND_OUTPUT ./socket_server2 $PORT 2>&1 &> /dev/null &		# Launch the server
	popd >> /dev/null
	ensureServerStarted $PORT

	# Run testclient
	echo -e "\tOk!"
	pushd client_temp_dir >> /dev/null
		$TIMEOUT $MAX_EXEC_TIME ./$TESTCLIENT "127.0.0.1" "$PORT" "$SMALL_TEST_FILE" 2>&1 &> /dev/null
		rc=$?
		#echo -e "\tReturn code = $rc"
	popd >> /dev/null
	
	# Kill valgrind
	echo "[TEST $2.$3] Killing Valgrind to get its output."
	for f in `ls -1 [0-9]*"_"$VALGRIND_OUTPUT`
	do
		valgrind_pid=`head -1 $f | cut -d= -f3`
		#cat $f
		kill -TERM $valgrind_pid 2>&1 &> /dev/null	# Kill valgrind to generate its output
	done

	killProcesses
	sleep 6

	rm -f valgrind 2>&1 &> /dev/null
	# More log files are possible, so concatenate them
	for f in `ls -1 [0-9]*"_"$VALGRIND_OUTPUT`
	do
		#cat $f
		cat $f >> valgrind
	done

	rm -f temp
	cat valgrind | grep "ERROR SUMMARY" | awk '{print $4}' | grep -Eo '[1-9]{1}|[1-9]{1}[0-9]{1,6}' > temp
	if [[ -s temp ]] ; then
		cp valgrind ./valgrind_output_$1.txt 2>&1 &> /dev/null
		echo -e "[Test $2.$3] Found memory leak. Test failed!\t\t\t\t\t\t\t[--TEST $2.$3 failed--]"
		echo -e "Valgrind output available in valgrind_output_$1.txt"
	else
		echo -e "[Test $2.$3] No memory leak. Test passed!!\t\t\t\t\t\t\t[++TEST $2.$3 passed++] Ok!"
	fi

	rm -f [0-9]*"_"$VALGRIND_OUTPUT 2>&1 &> /dev/null
	rm -f valgrind 2>&1 &> /dev/null

	rm -r -f temp_dir 2>&1 &> /dev/null
	rm -r -f client_temp_dir 2>&1 &> /dev/null
	killProcesses
}

cleanup
rm -f valgrind_output* 2>&1 &> /dev/null
rpcGen
compileTools
compileSource

return1=0
return2=0
return3=0
return4=0
TEST21PASSED=false

#********************************** TEST SUITE 1 ****************************************
echo -e "\n************* TESTING PART 1 *************"
test_suite=1

if [[ $TEST_01_PASSED == true ]] ; then
	
	testClientServerInteraction "$SERVER1" "$TESTCLIENT" "$SMALL_TEST_FILE" "1" "$test_suite" "1"
	
	testClientServerInteraction "$SERVER1" "$TESTCLIENT" "$BIG_TEST_FILE" "2" "$test_suite" "3"
	
	# Creating temp directories
	mkdir temp_dir 2>&1 &> /dev/null
	mkdir client_temp_dir 2>&1 &> /dev/null
	
	# Preparing the server
	cp -f $SOURCE_DIRECTORY/$SERVER1 temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/$PORTFINDER_EXEC temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/$SMALL_TEST_FILE temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/$BIG_TEST_FILE temp_dir 2>&1 &> /dev/null
	
	# Preparing the testclient
	cp -f $TOOLS_DIR/$TESTCLIENT client_temp_dir 2>&1 &> /dev/null
	
	# Run the student's server 1
	echo -n "Running student's server 1..."
	pushd temp_dir >> /dev/null
		student_server1_port=$(runServer "./$SERVER1")
		echo -e "\t[PORT $student_server1_port] Ok!"
	popd >> /dev/null
	
	# Run testclient with not existing file
	pushd client_temp_dir >> /dev/null
		$TIMEOUT $MAX_EXEC_TIME ./$TESTCLIENT "127.0.0.1" "$student_server1_port" "this_does_not_exist.txt" 2>&1 &> /dev/null
		return3=$?
		#echo -e -n "\tReturn code = $return3"
	popd >> /dev/null

	rm -r -f temp_dir 2>&1 &> /dev/null
	rm -r -f client_temp_dir 2>&1 &> /dev/null
	killProcesses
	
else
	echo -e "---Skipping part 1 because your server1 didn't compile :( ---"
fi
echo "************* END PART 1 *************"

#********************************** TEST SUITE 2 ****************************************
echo -e "\n\n************* TESTING PART 2 *************"
test_suite=2
TEST21PASSED=false
if [[ $TEST_02_PASSED == true ]] && [[ $TEST_01_PASSED == true ]] ; then

	testClientServerInteraction "$SERVER1" "$CLIENT" "$SMALL_TEST_FILE" "5" "$test_suite" "1"
	
	testClientServerInteraction "$TESTSERVER" "$CLIENT" "$SMALL_TEST_FILE" "5" "$test_suite" "3"
	
else
	echo -e "---Skipping part 2 because your client OR your server1 didn't compile---"
fi
echo "************* END PART 2 *************"

#********************************** TEST SUITE 3 ****************************************
echo -e "\n\n************* TESTING PART 3 *************"
test_suite=3
if [[ $TEST_03_PASSED == true ]] ; then
	
	# Creating temp directories
	mkdir temp_dir 2>&1 &> /dev/null
	mkdir client_temp_dir 2>&1 &> /dev/null
	
	# Preparing the server
	cp -f $SOURCE_DIRECTORY/$SERVER2 temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/$PORTFINDER_EXEC temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/$SMALL_TEST_FILE temp_dir 2>&1 &> /dev/null
	
	# Preparing the testclient
	cp -f $TOOLS_DIR/$TESTCLIENT client_temp_dir
	
	# Run the student's server 2
	echo -n "Running student's server 2..."
	pushd temp_dir >> /dev/null
		student_server2_port=$(runServer "./$SERVER2")
		echo -e "\t[PORT $student_server2_port] Ok!"
	popd >> /dev/null
	
	# Run testclient
	pushd client_temp_dir >> /dev/null
		$TIMEOUT $MAX_EXEC_TIME ./$TESTCLIENT "127.0.0.1" "$student_server2_port" "$SMALL_TEST_FILE" 2>&1 &> /dev/null
		return4=$?
	popd >> /dev/null

	rm -r -f temp_dir 2>&1 &> /dev/null
	rm -r -f client_temp_dir 2>&1 &> /dev/null
	killProcesses
	
	### Valgrind test ###
	testValgrind "$SERVER2" "$test_suite" "1"

else
	echo -e "---Skipping part 3---"
fi
echo "************* END PART 3 *************"

#**************************************** FINAL STEPS ************************************
echo -e "\n\n************* FINAL STEPS *************"
echo "return1 = $return1"
echo "return2 = $return2"
echo "return3 = $return3"
echo "return4 = $return4"
cleanup
rm -f temp
if [ "$return1" -eq 3 ] ; then
	echo -e "\n+++++ OK: You have met the minimum requirements to pass the exam!!! +++++\n"
	exit
fi

if [ "$return1" -gt 0 ] && [[ "$TEST21PASSED" == true ]] ; then
	echo -e "\n+++++ OK: You have met the minimum requirements to pass the exam!!! +++++\n"
	exit
fi

messages=0
echo -e "\n----- FAIL: You DO NOT have met the minimum requirements to pass the exam!!! -----\n"
if [ ! "$return1" -eq 3 ] ; then
	((messages++))
	echo -e "\n \t ${messages})  Your server1 didn't respond with the correct message in test 1.1\n "
fi
if [ ! "$return1" -gt 0 ] || [[ "$TEST21PASSED" == false ]] ; then
	((messages++))
	echo -e "\t ${messages})  Your client and  your server1 are not able to complete a file transfer \n "
fi
if [[ "$messages" == 2 ]] ; then
	echo -e "\n### Fix at least one of the two items to meet the minimum requirements ###\n "	
fi
echo -e "\n************* END test.sh *************\n"
