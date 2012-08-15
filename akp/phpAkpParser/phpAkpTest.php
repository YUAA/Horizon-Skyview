<?php
require 'phpAkpParser.php';

function reparseData($data)
{
    $parser = new AkpParser();
    for ($i = 0;$i < strlen($data);$i++)
    {
        if ($parser->parseTag($data[$i]))
        {
            if ($parser->tag == 'DD')
            {
                echo "Arbitrary data of length " . strlen($parser->data) . ".<br />";
                reparseData($parser->data);
            }
            else
            {
                echo $parser->tag . $parser->data . '<br />';
            }
        }
    }
}

$input = 'DD^00110011DD^00060006KL^:b0KL^:b0MS^hello glorious world!:bf';
//$input = 'DD^00010001LKL^:b0';

$parser = new AkpParser();
for ($i = 0;$i < strlen($input);$i++)
{
    if ($parser->parseTag($input[$i]))
    {
        if ($parser->tag == 'DD')
        {
            echo "Arbitrary data of length " . strlen($parser->data) . ".<br />";
            reparseData($parser->data);
        }
        else
        {
            echo $parser->tag . $parser->data . '<br />';
        }
    }
}
?>
