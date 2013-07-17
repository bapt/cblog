<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="fr">
	<head>
		<title><?cs var:title ?></title>
		<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
		<link href="/style.css" rel="stylesheet" type="text/css" />
		<link rel="shortcut icon" href="/favicon.ico" type="image/x-icon" />
		<link rel="icon" href="/favicon.ico" type="image/x-icon" />
		<?cs if:Query.tag ?>
		<link href="<?cs var:root ?>/tag/<?cs var:Query.tag ?>?feed=atom" rel="alternate" title="Atom 1.0 for tag <?cs var:Query.tag ?>" type="application/atom+xml" />
		<?cs else ?>
		<link href="/index.atom" rel="alternate" title="Atom 1.0" type="application/atom+xml" />
		<?cs /if ?>
	</head>
	<body>
		<div id="contener">
			<div id="header">
				<a href="<?cs var:url ?>"><?cs var:title ?></a> 
			</div>
			<div id="menu">
				<div class="menutitle">TAGS</div>
				<p class="tagcloud"><?cs each:tag = Tags ?><a href="<?cs var:root ?>/tag/<?cs var:tag.name ?>" rel="tag" style="white-space: nowrap;font-size: <?cs set:num = #79 + #5 * #tag.count ?><?cs var:num ?>%;"> <?cs var:tag.name ?></a> <?cs /each ?></p>
				<div class="menutitle">flux</div>
				<ul>
					<li class="syndicate"><a class="feed" href="<?cs var:CGI.ScriptName ?>?feed=atom">ATOM 1.0</a></li>
				</ul>
				<div class="menutitle">Links</div>
				<ul>
					<li><a href="http://www.freshports.org/search.php?stype=maintainer&amp;method=exact&amp;query=bapt@FreeBSD.org">My ports</a></li>
					<li><a href="http://fossil.etoilebsd.net/cplanet">CPlanet</a></li>
					<li><a href="http://fossil.etoilebsd.net/cblog">CBlog</a></li>
					<li><a href="http://fossil.etoilebsd.net/mohawk">mohawk</a></li>
					<li><a href="http://fossil.etoilebsd.net/poudriere">poudriere</a></li>
					<li><a href="http://github.com/pkgng/pkgng">pkgng</a></li>
				</ul>
				<div class="menutitle">Meta</div>
				<p>
					<a href="http://validator.w3.org/check?uri=referer"><img src="/resources/xhtml-valid.png" alt="Valid XHTML 1.0 Strict" width="80" height="15"/></a>
					<a href="http://jigsaw.w3.org/css-validator/check/referer"> <img src="/resources/css-valid.png" alt="Valid CSS!" width="80" height="15" /></a>
					<a href="http://www.freebsd.org/"><img src="/resources/freebsd.png" alt="FreeBSD Logo" width="80" height="15" /></a>
				</p>
			</div>
			<!-- div id menu -->
			<div id="content">
				<?cs if:err_msg ?>
				<h1 class="error">Error: <?cs var:err_msg ?></h1>
				<hr />
				<?cs /if ?>
				<?cs each:post = Posts ?>
				<div class="date"><?cs var:post.date ?></div>
				<!--<h2 class="storytitle"><a href="<?cs var:root ?>/post/<?cs var:string.slice(post.filename,0,string.find(post.filename,".txt")) ?>"><?cs var:post.title ?></a></h2>-->
				<h2 class="storytitle"><a href="<?cs var:root ?>/post/<?cs var:post.filename ?>"><?cs var:post.title ?></a></h2>
				<div class="tags"><?cs each:tag = post.tags ?><a href="<?cs var:root ?>/tag/<?cs var:tag.name ?>"><?cs var:tag.name ?></a> <?cs /each ?></div>
				<?cs if:Query.source ?>
				<pre><?cs var:post.source ?></pre>
				<?cs else ?><?cs var:post.html ?><?cs /if ?>
				<div class="comments"><a href="<?cs var:root ?>/post/<?cs var:post.filename ?>#comments"><?cs alt:post.nb_comments ?>0<?cs /alt ?> Comment(s)</a></div>
				<?cs if:subcount(Posts) == 1 && string.slice(CGI.RequestURI, 0, 5) == "/post" ?>
				<p id="comments" class="separator-story" />
				<?cs each:comment = post.comments ?>
				<?cs if:comment.url ?><a href="<?cs var:comment.url ?>"><?cs /if ?><?cs var:comment.author ?><?cs if:comment.url ?></a><?cs /if ?> wrote on <?cs var:comment.date ?> : <br />
				<p class="comment"><?cs var:html_strip(html_escape(text_html(comment.content))) ?></p>
				<br />
				<?cs /each ?>
				<?cs if:( Query.submit == "Preview") ?>
				<?cs if:Query.url ?><a href="<?cs var:Query.url ?>"><?cs /if ?><?cs var:Query.name ?><?cs if:Query.url ?></a><?cs /if ?> wrote <?cs var:Query.date ?> : <br />
				<p class="comment"><?cs var:html_strip(Query.comment) ?></p>
				<?cs /if ?>
				<?cs if:( post.allow_comments != "false") ?>
				<form method="post" action="<?cs var:CGI.RequestURI ?>" >
					<table>
						<tr>
							<td>Name :</td>
							<td><input name="name" size="35" value="<?cs var:Query.name ?>" /></td>
						</tr>
						<tr>
							<td>URL (optional) :</td>
							<td><input name="url" size="35" value="<?cs var:Query.url ?>" /></td>
						</tr>
						<tr>
							<td>Write here: "<?cs var:antispamres ?>"</td>
							<td><input name="antispam" size="35" value="" /></td>
						</tr>
						<tr>
							<td>Comments :</td>
							<td><textarea name="comment" rows="5" cols="60"><?cs var:Query.comment ?></textarea></td>
						</tr>
						<tr>
							<td><input type="hidden" name="test1" value="" /><input type="hidden" name="test2" value="cblog!powa" /><input type="submit" name="submit" value="Preview" /></td>
							<td><input type="submit" name="submit" value="Post" /></td>
						</tr>
					</table>
				</form>
				<?cs else ?>
				No comments allowed
				<?cs /if ?>
				<?cs /if ?>
				<p class="separator-story" />
					<?cs /each ?>
					<?cs if:subcount(Posts) != 1 ?>
				<div class="paging">
					<?cs if:nbpages ?>
					<p>Page<?cs if:(#nbpages >= 0) ?>s<?cs /if ?> : <?cs if:Query.page ?><?cs set:page = Query.page ?><?cs else ?><?cs set:page = #1 ?><?cs /if ?><?cs loop:x = #1, #nbpages, #1 ?> <?cs if:(#page == #x) ?><strong><?cs var:x ?></strong><?cs else ?> <a href="<?cs var:string.slice(CGI.RequestURI,0,string.find(CGI.RequestURI,"?")+1) ?>?page=<?cs var:x ?>"><?cs var:x ?></a><?cs /if ?><?cs /loop ?></p>
					<?cs /if ?>
				</div>
				<?cs /if ?>
			</div>
			<div id="footer">
				Powered by <a href="<?cs var:CBlog.url ?>"><?cs var:CBlog.version ?></a>
			</div>
		</div>
	</body>
</html>
