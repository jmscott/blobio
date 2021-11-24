#
#  Synopsis:
#	Write <div> help page for script service.
#  Source Path:
#	service.cgi
#  Source SHA1 Digest:
#	No SHA1 Calculated
#  Note:
#	service.d/help.pl was generated automatically by cgi2perl5.
#
#	Do not make changes directly to this script.
#

our (%QUERY_ARG);

print <<END;
<div$QUERY_ARG{id_att}$QUERY_ARG{class_att}>
END
print <<'END';
 <h1>Help Page for <code>/cgi-bin//service</code></h1>
 <div class="overview">
  <h2>Overview</h2>
  <dl>
<dt>Title</dt>
<dd>/cgi-bin/service</dd>
<dt>Synopsis</dt>
<dd>HTTP CGI Script /cgi-bin/service</dd>
<dt>Blame</dt>
<dd>jmscott</dd>
  </dl>
 </div>
 <div class="GET">
  <h2><code>GET</code> Request.</h2>
   <div class="out">
    <div class="handlers">
    <h3>Output Scripts in <code>$SERVER_ROOT/lib//service.d</code></h3>
    <dl>
     <dt>table</dt>
     <dd>
     </dd>
     <dt>table.del</dt>
     <dd>
     </dd>
     <dt>table.pg</dt>
     <dd>
<div class="query-args">
 <h4>Query Args</h4>
 <dl>
  <dt>srv</dt>
  <dd>
   <ul>
    <li><code>perl5_re:</code> [a-z][a-z0-9_-]{0,32}(,[a-z][a-z0-9_-]{0,32}){0,2}</li>
    <li><code>required:</code> yes</li>
   </ul>
  </dd>
  </dl>
</div>
     </dd>
  </dl>
 </div>
</div>
<div class="examples">
 <h3>Examples</h3>
 <dl>
   <dt><a href="/cgi-bin//service?/cgi-bin/service?help">
service?/cgi-bin/service?help</a></dt>
   <dd>Generate This Help Page for the CGI Script /cgi-bin/service</dd>
 </dl>
</div>
 </div>
</div>
END

1;
