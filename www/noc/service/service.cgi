<?xml version="1.0" encoding="UTF-8"?>
<cgi name="service">
 <title>/cgi-bin/service</title>
 <synopsis>HTTP CGI Script /cgi-bin/service</synopsis>
 <subversion id="$Id$" />
 <blame>jmscott</blame>
 <GET>
  <examples>
   <example
   	query="/cgi-bin/service?help"
   >
    Generate This Help Page for the CGI Script /cgi-bin/service
   </example>
  </examples>
  <out content-type="text/html">
   <putter name="table">
   </putter>
   <putter name="table.del">
   </putter>
   <putter name="table.pg">
    <query-args>
     <arg
       name="srv"
       perl5_re="[a-z][a-z0-9_-]{0,32}(,[a-z][a-z0-9_-]{0,32}){0,2}"
       required="yes"
     />
    </query-args>
   </putter>
  </out>
 </GET>

 <POST>
  <in>
   <putter name="post.add">
    <title>POST to Add a Service</title>
    <vars>
     <var
       name="srv"
       perl5_re="[a-z][a-z0-9_-]{0,32}"
       required="yes"
     />
     <var
       name="pghost"
       perl5_re="[a-z0-9][a-z0-9_.-]{0,63}"
       required="yes"
     />
     <var
       name="pgport"
       perl5_re="[0-9]{1,5}"
       required="yes"
     />
     <var
       name="pguser"
       perl5_re="[a-z0-9][a-z0-9_@-]{0,63}"
       required="yes"
     />
     <var
       name="pgdatabase"
       perl5_re="[a-z0-9][a-z0-9_-]{0,63}"
       required="yes"
     />
     <var
       name="blobsrv"
       perl5_re="[a-z0-9][a-z0-9]{0,7}:[[:graph:]]{1,127}"
       required="yes"
     />
     <var
       name="rrdhost"
       perl5_re="[a-z0-9][a-z0-9_.-]{0,63}"
       required="yes"
     />
     <var
       name="rrdport"
       perl5_re="[0-9]{1,5}"
       required="yes"
     />
    </vars>
   </putter>
   <putter name="post.del">
    <title>POST to Delete a Service</title>
    <vars>
     <var
       name="srv"
       perl5_re="[a-z][a-z0-9_-]{0,32}"
       required="yes"
     />
    </vars>
   </putter>
  </in>
 </POST>
</cgi>
