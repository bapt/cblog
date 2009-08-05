<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="fr" lang="fr">
<head><title><?cs var:title ?></title>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<link href="/style.css" rel="stylesheet" type="text/css" />
</head>
<body>
<div id="contener">
<div id="header">
<a href="<?cs var:url ?>"><?cs var:title ?></a> 
</div>
<div id="menu">
<div class="menutitle">TAGS</div>
<p class="tagcloud"><?cs each:tag = Tags ?><a href="<?cs var:CGI.ScriptName ?>?tag=<?cs var:tag.name ?>" rel="tag" style="white-space: nowrap;font-size: <?cs set:num = #79 + #5 * #tag.count ?><?cs var:num ?>%;"> <?cs var:tag.name ?></a> <?cs /each ?></p>
<div class="menutitle">flux</div>
<ul>
<li class="syndicate"><a class="feed" href="<?cs var:CGI.ScriptName ?>?feed=rss">RSS 1.0</a></li>
<li class="syndicate"><a class="feed" href="<?cs var:CGI.ScriptName ?>?feed=atom">ATOM</a></li>
</ul>
<div class="menutitle">Links</div>
<ul>
<li><a href="http://www.freshports.org/search.php?stype=maintainer&method=exact&query=baptiste.daroussin@gmail.com">Mes ports FreeBSD</a></li>
<li><a href="http://baptux.free.fr/wiki">Completion ZSH pour FreeBSD</a></li>
<li><a href="http://www.zshwiki.org/home/zen">ZSH Extended Network</a></li>
<li><a href="http://github.com/bapt/CPlanet/">CPlanet</a></li>
</ul>
<div class="menutitle">Meta</div>
<p>
<a href="http://validator.w3.org/check?uri=referer"><img src="/resources/xhtml-valid.png" alt="Valid XHTML 1.0 Strict" /></a>
<a href="http://jigsaw.w3.org/css-validator/check/referer"> <img src="/resources/css-valid.png" alt="Valid CSS!" /></a>
<a href="http://www.freebsd.org/"><img src="/resources/freebsd.png" alt="FreeBSD Logo" /></a>
</p>
</div><!-- div id menu -->
<div id="content">
<?cs each:post = Posts ?>
<div class="date"><?cs var:post.date ?></div>
<!--<h2 class="storytitle"><a href="?post=<?cs var:string.slice(post.filename,0,string.find(post.filename,".txt")) ?>"><?cs var:post.title ?></a></h2>-->
<h2 class="storytitle"><a href="?post=<?cs var:post.filename ?>"><?cs var:post.title ?></a></h2>
<div class="tags"><?cs each:tag = post.tags ?><a href="?tag=<?cs var:tag.name ?>"><?cs var:tag.name ?></a> <?cs /each ?></div>
<?cs var:post.content ?>
<div class="comments"><?cs alt:post.nb_comments ?>0<?cs /alt ?> commentaire(s)</div>
<?cs if:Query.post ?>
<p class="separator-story" />
<?cs each:comment = post.comments ?>
<?cs if:comment.url ?><a href="<?cs var:comment.url ?>"><?cs /if ?><?cs var:comment.author ?><?cs if:comment.url ?></a><?cs /if ?> a écrit le <?cs var:comment.date ?> : <br />
<blockquote><?cs var:html_strip(html_escape(text_html(comment.content))) ?></blockquote>
<br />
<?cs /each ?>
<?cs if:( Query.submit == "Preview") ?>
<?cs if:Query.url ?><a href="<?cs var:Query.url ?>"><?cs /if ?><?cs var:Query.name ?><?cs if:Query.url ?></a><?cs /if ?> a écrit le <?cs var:Query.date ?> : <br />
<blockquote><?cs var:html_strip(Query.comment) ?></blockquote>
<?cs /if ?>
<form method="POST" action="<?cs var:CGI.ScriptName ?>?post=<?cs var:post.filename ?>" >
<table>
<tr><td>Nom :</td><td><input name="name" size="35" value="<?cs var:Query.name ?>"></td></tr>
<tr><td>URL (optionel) :</td><td><input name="url" size="35" value="<?cs var:Query.url ?>"></td></tr>
<tr><td>Commentaires :</td><td><textarea name="comment" rows="5" cols="60"><?cs var:Query.comment ?></textarea></td></tr>
<tr><td><input type="hidden" name="test1" value=""><input type="submit" name="submit" value="Preview"></td><td><input type="submit" name="submit" value="Post"></td></tr>
</table>
<?cs /if ?>
<p class="separator-story" />
<?cs /each ?>
<div class="paging">
<p>Page<?cs if:(#nbpages >= 0) ?>s<?cs /if ?> : <?cs if:Query.page ?><?cs set:page = Query.page ?><?cs else ?><?cs set:page = #1 ?><?cs /if ?><?cs loop:x = #1, #nbpages, #1 ?> <?cs if:(#page == #x) ?><strong><?cs var:x ?></strong><?cs else ?> <a href="?<?cs if:Query.tag ?>tag=<?cs var:Query.tag ?>&<?cs /if ?>page=<?cs var:x ?>"><?cs var:x ?></a><?cs /if ?><?cs /loop ?></p>
</div>
</div>
<div id="footer">
</div>
</div>
</body>
</html>
