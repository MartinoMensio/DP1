#!/bin/bash
SOURCE_DIRECTORY="source"
GCC_OUTPUT="gcc_output.txt"
TOOLS_DIR="tools"
PORTFINDER_DIR="portfinder"
PORTFINDER_EXEC="port_finder"
OUTPUT_FILE="output.txt"

# Command to avoid infinite loops
TIMEOUT="timeout"

# Maximum allowed running time of a standard client (to avoid infinite loops)
MAX_EXEC_TIME=30

#*******************************KILL PROCESSES**********************************
function killProcesses
{
	# Kill running servers
	for f in `ps -ef | grep socket_server1 | awk '{print $2}'`; do kill -9 $f &> /dev/null; done
	for f in `ps -ef | grep socket_server2 | awk '{print $2}'`;	do kill -9 $f &> /dev/null; done

	# Kill running clients
	for f in `ps -ef | grep socket_client | awk '{print $2}'`; do kill -9 $f 2>&1 &> /dev/null; done
	for f in `ps -ef | grep testclient | awk '{print $2}'`; do kill -9 $f 2>&1 &> /dev/null; done
}

#**********************************CLEANUP***************************************
function cleanup
{
	echo -n "Cleaning up..."
	killProcesses
	# Delete previously generated folders and files (if they exist)
	rm -r -f temp* 2>&1 &> /dev/null
	rm -r -f client_temp_dir 2>&1 &> /dev/null
	rm -f $TOOLS_DIR/$PORTFINDER_EXEC
	rm -f $SOURCE_DIRECTORY/socket_client
	rm -f $SOURCE_DIRECTORY/socket_server1
	rm -f $SOURCE_DIRECTORY/socket_server2
	rm -f big_file.txt
	echo -e "\t\t\tOk!"
}

#***************************COMPILING TESTING TOOLS******************************
function compileTools
{
	echo -n "Compiling testing tools..."

	gcc -g -DTRACE -Wno-format-security -o $TOOLS_DIR/$PORTFINDER_EXEC $TOOLS_DIR/$PORTFINDER_DIR/*.c $TOOLS_DIR/*.c 2> /dev/null

	if [ ! -e $TOOLS_DIR/$PORTFINDER_EXEC ] || [ ! -e $TOOLS_DIR/testclient ] ; then
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
	gcc -g -o socket_server1 server1/*.c *.c -Iserver1 -lpthread -lm >> $GCC_OUTPUT 2>&1 
	if [ ! -e socket_server1 ] ; then
		TEST_01_PASSED=false
		echo -e "\tFail! \n\t[ERROR] Unable to compile source code for server1."
		echo -e "\tGCC log is available in $SOURCE_DIRECTORY/$GCC_OUTPUT"
	else
		TEST_01_PASSED=true
		echo -e "\tOk!"
	fi

	echo -n "Test 0.2 (compiling client)...";
	gcc -g -o socket_client client/*.c *.c -Iclient -lpthread -lm >> $GCC_OUTPUT 2>&1 
	if [ ! -e socket_client ] ; then
		TEST_02_PASSED=false
		echo -e "\tFail! \n\t[ERROR] Unable to compile source code for the client."
		echo -e "\tGCC log is available in $SOURCE_DIRECTORY/$GCC_OUTPUT"
	else
		TEST_02_PASSED=true
		echo -e "\tOk!"
	fi

	echo -n "Test 0.3 (compiling server2)...";
	gcc -g -o socket_server2 server2/*.c *.c -Iserver2 -lpthread -lm >> $GCC_OUTPUT 2>&1 
	if [ ! -e socket_server2 ] ; then
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
		rm -f temp
		netstat -ln | grep "tcp" | grep ":$1 " > temp
		if [[ -s temp ]] ; then
			# Server started
			break
		else
			# Server non started
			sleep 1
		fi
	done
	rm -f temp
}

cleanup
compileTools
compileSource

return1=0
return2=0

#********************************** TEST SUITE 1 ****************************************
echo -e "\n************* TESTING PART 1 *************"
test_suite=1
if [[ $TEST_01_PASSED == true ]] ; then
	mkdir temp_dir 2>&1 &> /dev/null
	mkdir client_temp_dir 2>&1 &> /dev/null
	cp -f $SOURCE_DIRECTORY/socket_server1 temp_dir
	cp -f $TOOLS_DIR/$PORTFINDER_EXEC temp_dir
	cp -f $TOOLS_DIR/testclient client_temp_dir
	cp -f $TOOLS_DIR/big_file.txt client_temp_dir
	# Run the student's server 1
	echo -n "Running student's server 1..."
	pushd temp_dir >> /dev/null
		student_server1_port=$(runServer "./socket_server1" "1")
		echo -e "\t[PORT $student_server1_port] Ok!"
	popd >> /dev/null
	echo -n "[TEST $test_suite.1] Launching test client on port $student_server1_port..."
	pushd client_temp_dir >> /dev/null
		$TIMEOUT $MAX_EXEC_TIME ./testclient "127.0.0.1" "$student_server1_port" "1" "big_file.txt" 2>&1 &> /dev/null
		rc=$?
		echo -e "\tReturn code = $rc"
		return1=$rc
	popd >> /dev/null
	if (( $return1 == 3 )) ; then
		echo -e "[++TEST $test_suite.1 passed++] Ok!"	

	else
		echo -e "[--TEST $test_suite.1 FAILED--] Your server1 didn't respond correctly."  
	fi

	rm -r -f temp_dir 2>&1 &> /dev/null
	rm -r -f client_temp_dir 2>&1 &> /dev/null
	rm -f big_file.txt
	killProcesses

else
	echo -e "---Skipping part 1---"
fi
echo "************* END PART 1 *************"

#********************************** TEST SUITE 2 ****************************************
echo -e "\n\n************* TESTING PART 2 *************"
test_suite=2
killProcesses
TEST21PASSED=false
if [[ $TEST_02_PASSED == true ]] ; then
	if [[ $TEST_01_PASSED == true ]] ; then
		mkdir temp_dir 2>&1 &> /dev/null
		mkdir client_temp_dir 2>&1 &> /dev/null
		cp -f $SOURCE_DIRECTORY/socket_server1 temp_dir
		cp -f $TOOLS_DIR/$PORTFINDER_EXEC temp_dir
		cp -f $SOURCE_DIRECTORY/socket_client client_temp_dir
		cp -f $TOOLS_DIR/big_file.txt client_temp_dir
		# Run the student's server 1
		echo -n "Running student's server 1..."
		pushd temp_dir >> /dev/null
			student_server1_port=$(runServer "./socket_server1" "1")
		popd >> /dev/null
		echo -e "\t[PORT $student_server1_port] Ok!"
		echo -n "[TEST $test_suite.1] Launching student client on port $student_server1_port..."
		pushd client_temp_dir >> /dev/null
			$TIMEOUT $MAX_EXEC_TIME ./socket_client "127.0.0.1" "$student_server1_port" "1" "big_file.txt" > client_output.txt
			rc=$?
			echo -e "\tReturn code = $rc"
		popd >> /dev/null
		
		expected_result=`cat $TOOLS_DIR/expected_result.txt`

		if [ -e client_temp_dir/client_output.txt ] ; then
			if grep -q "$expected_result" client_temp_dir/client_output.txt ; then
				echo -e "[++TEST $test_suite.1 passed++] Ok!"
				TEST21PASSED=true
			else
				echo -e "[--TEST $test_suite.1 FAILED--] Your client printed a wrong result on the standard output." 
			fi
		else
			echo -e "[--TEST $test_suite.1 FAILED--] Your client printed a wrong result on the standard output." 
		fi
 
		rm -r -f temp_dir 2>&1 &> /dev/null
		rm -r -f client_temp_dir 2>&1 &> /dev/null
	else
		echo -e "---Skipping part 2.1---"
	fi
else
	echo -e "---Skipping part 2---"
fi
echo "************* END PART 2 *************"

#********************************** TEST SUITE 3 ****************************************
echo -e "\n\n************* TESTING PART 3 *************"
test_suite=3
killProcesses
if [[ $TEST_03_PASSED == true ]] ; then
	mkdir temp_dir 2>&1 &> /dev/null
	mkdir client_temp_dir 2>&1 &> /dev/null
	cp -f $TOOLS_DIR/testclient client_temp_dir
	cp -f $SOURCE_DIRECTORY/socket_server2 temp_dir
	cp -f $TOOLS_DIR/$PORTFINDER_EXEC temp_dir
	cp -f $TOOLS_DIR/big_file.txt client_temp_dir
	# Run the student's server 2
	echo -n "Running student's server 2..."
	pushd temp_dir >> /dev/null
		student_server2_port=$(runServer "./socket_server2" "1")
	popd >> /dev/null
	echo -e "\t[PORT $student_server2_port] Ok!"
	echo -n "[TEST $test_suite.1] Launching test client on port $student_server2_port..."
	pushd client_temp_dir >> /dev/null
		$TIMEOUT $MAX_EXEC_TIME ./testclient "127.0.0.1" "$student_server2_port" "1" "big_file.txt" 2>&1 &> /dev/null
		rc=$?
		echo -e "\tReturn code = $rc"
		return2=$rc
	popd >> /dev/null
	if (( $return2 == 3 )) ; then
		echo -e "[++TEST $test_suite.1 passed++] Ok!"	
	else
		echo -e "[--TEST $test_suite.1 FAILED--] Your server2 didn't respond correctly."  
	fi

	rm -r -f temp_dir 2>&1 &> /dev/null
	rm -r -f client_temp_dir 2>&1 &> /dev/null

else
	echo -e "---Skipping part 3---"
fi
echo "************* END PART 2 *************"


#**************************************** FINAL STEPS ************************************
echo -e "\n\n************* FINAL STEPS *************"
cleanup
rm -f temp
if [ $return1 -eq 3 ] ; then
	echo -e "\n+++++ OK: You have met the minimum requirements to pass the exam!!! +++++\n"
	exit
fi

if [ $return1 -gt 1 ] && [[ $TEST21PASSED == true ]]  ; then
	echo -e "\n+++++ OK: You have met the minimum requirements to pass the exam!!! +++++\n"
	exit
fi

messages=0
echo -e "\n----- FAIL: You DO NOT have met the minimum requirements to pass the exam!!! -----\n"
if [ ! $return1 -eq 3 ] ; then
	((messages++))
	echo -e "\n \t ${messages})  Your server1 didn't respond with the correct message in test 1.1\n "
fi
if [[ $TEST21PASSED == false ]] ; then
	((messages++))
	echo -e "\t ${messages})  Your client and server1 are not able to complete a file transfer and/or transmit the correct hash code\n "
fi
if [[ $messages == 2 ]] ; then
	echo -e "\n### Fix at least one of the two items to meet the minimum requirements ###\n "	
fi
echo -e "\n************* END test.sh *************\n"
