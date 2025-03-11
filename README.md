# simgrid-examples
Various example codes for SimGrid.

* simgrid_cluster_with_errors
This example sets up a single cluster with a few hosts and executes some jobs on the cluster. Job failures during running are simulated. At the end of running,
a summary is displayed with the number of finished and failed jobs. The code is using the simple Mailbox method.

The code is based on SimGrid version 3.36.

Compile the code with
<code>
g++ -std=c++17 simgrid_cluster_with_errors.cpp -o simgrid_cluster -Wl,-rpath,/usr/local/lib -lsimgrid
</code>
(assuming a local SimGrid installation in /usr/local/lib.)

Run the code with
<code>
./simgrid_cluster
</code>
