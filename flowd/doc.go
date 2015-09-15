// Flow blob request records into a directed acylic graph of processes.
// 
// The flowd server discovers immutable facts about blobs by tailing a
// stream of blob request records - see blobio/biod/README - from a unix
// file. The tailed request record is then sent to a set of rules that
// fire either unix processes or sql queries ordered in a directed acyclic
// graph called a "flow".  The results/side-effects of the fired
// process/queries are facts about the blob stored outside of flowd,
// typically in a relational database.
// 
// The design inspiration of flowd is a mashup of the famous unix commands
// "make" and "awk".  The exit status of a called process can be qualified
// upon to trigger subsequent calls other processes in the flow.
// 
// The invocation of a particular process is determined by qualifying onw
// the exit status of invoked processes durint the the same flow.  A flow
// on a particular brr tuple will always terminate, since the firing order
// is a DAG.  However, infinite loops are possible for processes that
// always reread a blob, thus generating more brr records.
// 
// Flows are independent of other flows.  In other words, the results of a
// particular flow can not be referenced with in flowd by a different flow.
// 
// The flow diagram of the software is as follows:
// 
// tail <- brr source (spool/biod.brr or fifo)
//   parse brr record, skipping corrupted records
//     -> brr flow work queue
//       flow
//  Blame:
//	jmscott@setspace.com
//	setspace@gmail.com
package main;
