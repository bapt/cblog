<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
	<title type="text"><?cs var:title ?></title>
	<subtitle type="text"><?cs var:title ?></subtitle>
	<id><?cs var:url ?>/</id>
	<link rel="self" type="text/xml" href="<?cs var:url ?>?feed=atom" />
	<link rel="alternate" type="text/html" href="<?cs var:url ?>" />
	<updated><?cs var:gendate ?></updated>
	<author><name>Bapt</name></author>
	<generator uri="http://wiki.github.com/bapt/CBlog" version="0.1 alpha">CBlog</generator>
	<?cs each:post = Posts ?>
	<entry>
		<title type="text"><?cs var:post.title ?></title>
		<author><name>Bapt</name></author>
		<content type="html"><![CDATA[<?cs var:post.html ?>]]></content>
		<?cs each:tag = post.tags ?><category term="<?cs var:tag.name ?>" /><?cs /each ?>
		<id><?cs var:post.filename ?></id>
		<link rel="alternate" href="<?cs var:url ?>post/<?cs var:post.filename ?>" />
		<updated><?cs var:post.date ?></updated>
		<published><?cs var:post.date ?></published>
	</entry>
	<?cs /each ?>
</feed>
