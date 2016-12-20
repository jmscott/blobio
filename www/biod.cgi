<?xml version="1.0" encoding="UTF-8"?>
<cgi name="biod">
 <title>/cgi-bin/biod</title>
 <synopsis>HTTP CGI Script /cgi-bin/biod</synopsis>
 <subversion id="$Id$" />
 <blame>jmscott</blame>
 <GET>
  <examples>
   <example
   	query="/cgi-bin/biod?help"
   >
    Generate This Help Page for the CGI Script /cgi-bin/biod
   </example>
  </examples>

  <out content-type="text/html">
   <putter name="table">
    <query-args>
     <arg
       name="since"
       perl5_re="1hr|6hr|today|yesterday|24hr|1week|1month"
       default="1hr"
     />
    </query-args>
   </putter>
  </out>

 </GET>
</cgi>
