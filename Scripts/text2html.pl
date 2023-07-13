use HTML::TextToHTML;

# create a new object
my $conv = new HTML::TextToHTML();

# convert a file
$conv->txt2html(infile=>[$ARGV[0]],
                 outfile=>$ARGV[1],
                 title=>"Changes",
                 mail=>1,
  );
 