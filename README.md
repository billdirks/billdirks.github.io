# Write the blog

Put `.md` file in `pelican/content/<my_blog_dir>`

To serve locally:

```
make html && make serve
```


# Publish a blog

```
cd pelican
make clean
make publish
# Instead of rsync, one could delete ../docs and mv output there.
rsync -avu output/ ../docs
```

Then make a PR, merge it into the `publish` branch, and it will publish.
