#!/usr/bin/ruby

# tests ability of your peer to send a correct WHOHAS request
def test1

        cp_pid = fork do
            exec("./cp1_tester -p nodes.map -c A.chunks -f C.chunks -m 4 -i 1 -t 1 -d 15")
        end

	parent_to_child_read, parent_to_child_write = IO.pipe
	peer_pid = fork do
	    parent_to_child_write.close

	    $stdin.reopen(parent_to_child_read) or
			raise "Unable to redirect STDIN"

	    exec("./peer -p nodes.map -c B.chunks -f C.chunks -m 4 -i 2 -d 15")
	end
	parent_to_child_read.close

	sleep 1.0
	## send message to standard in of peer
	write_to_peer = "GET A.chunks silly.tar\n"
	parent_to_child_write.write(write_to_peer)
	parent_to_child_write.flush

	## wait for checkpoint1 binary to stop
	pid = Process.waitpid(cp_pid)
	return_code = $? >> 8;

	Process.kill("SIGKILL", peer_pid);

	if(pid == cp_pid)
		if (return_code == 10) 
			return 1.0
		else
			return 0.0
		end
	else
		return 0.0
	end
end

# tests ability of peer to respond to a WHOHAS with an IHAVE they have all files requested
def test2

	peer_pid = fork do

	    exec("./peer -p nodes.map -c A.chunks -f C.chunks -m 4 -i 2 -d 15")    
	end

	sleep 1.0

        cp_pid = fork do
            exec("./cp1_tester -p nodes.map -c B.chunks -f C.chunks -m 4 -i 1 -t 2 -d 15")
        end

	## wait for checkpoint1 binary to stop
	pid = Process.waitpid(cp_pid)
	return_code = $? >> 8;

	Process.kill("SIGKILL", peer_pid);

	if(pid == cp_pid)
		if (return_code == 10)
			return 1.0
		else
			return 0.0
		end
	else
		puts "Error running test2.  Other process " + pid.to_s + 
		" exited before checkpoint process with code " + return_code.to_s
		puts "##########  Test 2 Failed #########\n"
		return 0.0
	end

end


# Makes sure that you don't send back an IHAVE, if you don't have any of the
# chunks requested in a WHOHAS.  In this case, both peers are configured
# to have B.chunks, and the checkpoint peer will send a request for A.chunks
def test3

	peer_pid = fork do

	    exec("./peer -p nodes.map -c A.chunks -f C.chunks -m 4 -i 2 -d 15")    
	end

	sleep 1.0

        cp_pid = fork do
            exec("./cp1_tester -p nodes.map -c A.chunks -f C.chunks -m 4 -i 1 -t 3 -d 15")

        end

	## wait for checkpoint1 binary to stop
	pid = Process.waitpid(cp_pid)
	return_code = $? >> 8;
	Process.kill("SIGKILL", peer_pid);
	if(pid == cp_pid)
		if (return_code == 10) 
			return 1.0
		else
			return 0.0
		end
	else
		puts "Error running test3.  Other process " + pid.to_s + 
		" exited before checkpoint process with code " + return_code.to_s
		puts "##########  Test 3 Failed #########\n"
		Process.kill("SIGKILL", cp_pid)
                return 0.0
	end

end

# here's where we actually run the tests


if (!File.exists?("peer"))
	puts "Error: need to have binary named 'peer' in this directory"
	exit 1
end 


## CHANGE ME! 
spiffy_port = 15440


ENV['SPIFFY_ROUTER'] = "127.0.0.1:#{spiffy_port}"   

puts "starting SPIFFY on port #{spiffy_port}"

spiffy_pid = fork do
	exec("perl hupsim.pl -m topo.map -n nodes.map -p #{spiffy_port} -v 0")    
end

puts "starting tests"

test1_score = test1

sleep 3.0

test2_score = test2

sleep 3.0

test3_score = test3

Process.kill("SIGKILL", spiffy_pid)
puts "test1_score #{test1_score}"
puts "test2_score #{test2_score}"
puts "test3_score #{test3_score}"
