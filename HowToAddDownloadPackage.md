How To: Add Download Package (tar.gz)

  1. Commit changes to sockperf on google code: 'svn ci'
  1. Check out clean sockperf: 'svn co https://sockperf.googlecode.com/svn/branches/sockperf_v2 sockperf'
  1. cd sockperf && ./autogen.sh && ./configure && make
  1. Shrink to .tar.gz: using 'make dist'
  1. Upload as new Download package to google docs with some details about the major changes
  1. Add the major changes to the wiki page