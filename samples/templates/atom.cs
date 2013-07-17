<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
	<title type="text"><?cs var:title ?></title>
	<subtitle type="text"><?cs var:title ?></subtitle>
	<id><?cs var:url ?>/</id>
	<link rel="self" href="<?cs var:url ?><?cs var:CGI.RequestURI ?>" />
	<link rel="alternate" type="text/html" href="<?cs var:url ?>" />
	<updated><?cs var:gendate ?></updated>
	<author><name><?cs var:author ?></name></author>
	<generator uri="<?cs var:CBlog.url ?>" version="<?cs var:CBlog.version ?>"><?cs var:CBlog.version ?></generator>
	<?cs each:post = Posts ?>
	<entry>
		<title type="text"><?cs var:post.title ?></title>
		<author><name><?cs var:author ?></name></author>
		<content type="html"><?cs var:html_escape(post.html) ?></content>
		<?cs each:tag = post.tags ?><category term="<?cs var:tag.name ?>" /><?cs /each ?>
		<id><?cs var:url ?>post/<?cs var:post.filename ?></id>
		<link rel="alternate" href="<?cs var:url ?>/post/<?cs var:post.filename ?>" />
		<updated><?cs var:post.date ?></updated>
		<published><?cs var:post.date ?></published>
	</entry>
	<?cs /each ?>
</feed>
