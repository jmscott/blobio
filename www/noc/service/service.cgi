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
  </out>
 </GET>
 <POST>
  <in>
   <putter name="post.add">
    <title>POST a search query</title>
    <vars>
     <var
       name="srvtag"
       perl5_re="[a-z][a-z0-9_-]{0,32}"
     />
     <var
       name="pghost"
       perl5_re="[a-z0-9][a-z0-9_.-]{0,63}"
     />
     <var
       name="pgport"
       perl5_re="[0-9]{1,5}"
     />
     <var
       name="pguser"
       perl5_re="[a-z0-9][a-z0-9_@-]{0,63}"
     />
     <var
       name="pgdatabase"
       perl5_re="[a-z0-9][a-z0-9_-]{0,63}"
     />
     <var
       name="blobsrv"
       perl5_re="[a-z0-9][a-z0-9]{0,7}:[[:graph:]]{1,127}"
     />
     <var
       name="rrdhost"
       perl5_re="[a-z0-9][a-z0-9_.-]{0,63}"
     />
     <var
       name="rrdport"
       perl5_re="[0-9]{1,5}"
     />
    </vars>
   </putter>
  </in>
 </POST>
</cgi>
