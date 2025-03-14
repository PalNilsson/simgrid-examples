# simgrid-examples
Various example codes for SimGrid.

<b>simgrid_cluster_with_errors</b>:
This example sets up a single cluster with a few hosts and executes some jobs on the cluster. Job failures during running are simulated. At the end of running,
a summary is displayed with the number of finished and failed jobs. The code is using master and worker actors and communication is via a Mailbox.

The code is based on SimGrid version 3.36.

Compile the code with
<code>
g++ -std=c++17 simgrid_cluster_with_errors.cpp -o simgrid_cluster_errors -Wl,-rpath,/usr/local/lib -lsimgrid
</code>
(assuming a local SimGrid installation in /usr/local/lib).

Run the code with
<code>
./simgrid_cluster_errors
</code>

<b>simgrid_cluster_with_historical_errors</b>:
This example is using the previous example but is randomizing errors using real historical PanDA pilot errors. The JSON file with the errors is currently provided (error_codes.json).

The code is based on SimGrid version 3.36.

Compile the code with
<code>
g++ -std=c++17 simgrid_cluster_with_historical_errors.cpp -o simgrid_cluster_historical_errors -Wl,-rpath,/usr/local/lib -lsimgrid
</code>
(assuming a local SimGrid installation in /usr/local/lib.)

Run the code with
<code>
./simgrid_cluster_historical_errors --input \<input data\> --n \<number of jobs\> --queue \<queue name\> \[--mute\]
</code>

Note: In case of trouble with boost headers, find where they are and add the corresponding -I/opt/homebrew/opt/boost/include compiler flag.
