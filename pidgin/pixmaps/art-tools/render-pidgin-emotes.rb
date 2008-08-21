#!/usr/bin/env ruby

require "rexml/document"
require "ftools"
include REXML
INKSCAPE = '/usr/bin/inkscape'
SRC = "./svg"

def renderit(file)
  svg = Document.new(File.new("#{SRC}/#{file}", 'r'))
  svg.root.each_element("//g[contains(@inkscape:label,'plate')]") do |icon|
     filename = icon.attributes["label"]
     filename = `echo -n #{filename} | sed -e 's/plate\-//g'`
     puts "#{file} #{filename}.png"
     icon.each_element("rect") do |box|
       if box.attributes['inkscape:label'] == '22x22'
           dir = "#{box.attributes['width']}x#{box.attributes['height']}/"
           cmd = "#{INKSCAPE} -i #{box.attributes['id']} -e #{dir}/#{filename}.png #{SRC}/#{file} > /dev/null 2>&1"
           File.makedirs(dir) unless File.exists?(dir)
           system(cmd)
           print "."
       elsif box.attributes['inkscape:label'] == '24x24'
           dir = "#{box.attributes['width']}x#{box.attributes['height']}/"
           cmd = "#{INKSCAPE} -i #{box.attributes['id']} -e #{dir}/#{filename}.png #{SRC}/#{file} > /dev/null 2>&1"
           File.makedirs(dir) unless File.exists?(dir)
           system(cmd)
           print "."
       end
     end
     puts ''
  end
end

if (ARGV[0].nil?) #render all SVGs
  puts "Rendering from SVGs in #{SRC}"
  Dir.foreach(SRC) do |file|
    renderit(file) if file.match(/svg$/)
  end
  puts "\nrendered all SVGs"
else #only render the SVG passed
  file = "#{ARGV[0]}.svg"
  if (File.exists?("#{SRC}/#{file}"))
    renderit(file)
    puts "\nrendered #{file}"
  else
    puts "[E] No such file (#{file})"
  end
end
