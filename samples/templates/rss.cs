<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:atom="http://www.w3.org/2005/Atom">
	<channel>
		<title><?cs var:title ?></title>
		<link><?cs var:url ?></link>
		<atom:link rel="self" href="<?cs var:url ?><?cs var:CGI.RequestURI ?>" />
		<description><?cs var:title ?></description>
		 <language>fr</language>
		 <generator>CBlog 0.1 alpha</generator>
		 <lastBuildDate><?cs var:gendate ?></lastBuildDate>
		 <?cs each:post = Posts ?>
		 <item>
			 <title><?cs var:post.title ?></title>
			 <description>[CDATA[<?cs var:post.htme ?>]]</description>
			 <link><?cs var:url ?>/post/<?cs var:post.filename ?></link>
			 <guid><?cs var:url ?>/post/<?cs var:post.filename ?></guid>
			 <?cs each:tag = post.tags ?>
			 <category><?cs var:tag.name ?></category>
			 <?cs /each ?>
			 <pubDate><?cs var:post.date ?></pubDate>
		 </item>
		 <?cs /each ?>
	</channel>
</rss>
