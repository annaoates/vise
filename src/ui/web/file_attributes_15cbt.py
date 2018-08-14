##
## VISE Customized for 15th Century Booktrade Project
##
## Author: Abhishek Dutta <adutta@robots.ox.ac.uk>
## 25 Jan. 2018
##

import cherrypy;

import random;
import template;
import copy;

import pandas as pd;
import json;

class file_attributes_15cbt:
    
  def __init__(self, pageTemplate, docMap, pathManager_obj, file_attributes_fn= None, file_attributes_filename_colname= "filename", istc_db_fn=None, istc_id_colname="id", region_attributes_fn=None, region_attributes_filename_colname="filename", preprocess_fn=None, preprocess_filename_colname=None):
    self.pT= pageTemplate;
    self.docMap= docMap;
    self.pathManager_obj= pathManager_obj;
    self.dsetname= self.docMap.keys()[0];

    self.attributes_available = False;
    self.file_attributes_index = None;
    self.filename_to_docid_map = {};
    self.istc_db = None;
    self.region_attributes = None;
    self.image_scale_factor = None;

    self.load_istc_db(istc_db_fn, istc_id_colname);
    self.load_file_attributes(file_attributes_fn, file_attributes_filename_colname);
    self.load_region_attributes(region_attributes_fn, region_attributes_filename_colname);
    #self.load_image_scale_factor(preprocess_fn, preprocess_filename_colname);

  #
  # @todo: scale image region to resized image
  #
  def load_image_scale_factor(self, preprocess_fn, preprocess_filename_colname):
    #image_fn,original_size,original_width,original_height,tx_width,tx_height
    if preprocess_fn != None:
      file_count = len(self.docMap[self.dsetname]);  
      self.image_scale_factor = {};
      self.image_scale_factor['doc_id'] = range(0, file_count);
      self.image_scale_factor['filename'] = list();
      self.image_scale_factor['scale_factor'] = list();

      preprocess_data = pd.read_csv(preprocess_fn, sep=',', header=0, engine='python', encoding='utf-8');
      preprocess_data.rename( columns={preprocess_filename_colname: 'filename'}, inplace=True );
      for doc_id in range(0,file_count):
        filename = self.pathManager_obj[self.dsetname].displayPath(doc_id);
        self.image_scale_factor['filename'].append(filename);
        #print preprocess_data['filename']
        print filename
        print( preprocess_data.columns)
        print preprocess_data.loc[ preprocess_data['filename'] == 'ib00297900_02001216_A1r.jpg' ]

        print row
        if row is not None:
          self.image_scale_factor['scale_factor'].append( row['original_width'] / row['tx_width'] );
        else:
          self.image_scale_factor['scale_factor'].append(1.0);

      print 'Finished loading %d image scale factor' % (self.image_scale_factor.shape[0]);
      print self.image_scale_factor;

  def load_region_attributes(self, region_attributes_fn, region_attributes_filename_colname):
    #filename,file_size,file_attributes,region_count,region_id,region_shape_attributes,region_attributes
    if region_attributes_fn != None:
      self.region_attributes = pd.read_csv(region_attributes_fn, sep=',', header=0, engine='python', encoding='utf-8');
      self.region_attributes.drop([col for col in self.region_attributes.columns if "Unnamed" in col], axis=1, inplace=True) # remove unnamed columns
      self.region_attributes.rename( columns={region_attributes_filename_colname: 'filename'}, inplace=True );
      print 'Finished loading %d region attributes' % (self.region_attributes.shape[0]);

  def load_istc_db(self, istc_db_fn, istc_id_colname):
    if istc_db_fn != None:
      self.istc_db = pd.read_csv(istc_db_fn, encoding='utf-8', keep_default_na=False, na_values='', dtype={"id":str,"author":str,"title":str,"imprint":str,"format":str,"imprint_year":int});
      self.istc_db.drop([col for col in self.istc_db.columns if "Unnamed" in col], axis=1, inplace=True) # remove unnamed columns
      if istc_id_colname != 'id':
        self.istc_db.rename( columns={istc_id_colname: 'id'}, inplace=True );
      print "Loaded %d entries in istc database " % (self.istc_db.shape[0]);

  def load_file_attributes(self, file_attributes_fn=None, file_attributes_filename_colname=None):
    file_count = len(self.docMap[self.dsetname]);
    dataset_index = {};
    dataset_index['doc_id'] = range(0, file_count);
    dataset_index['filename'] = list();
    dataset_index['istc_id'] = list();
    dataset_index['mei_id'] = list();
    dataset_index['folio'] = list();
    dataset_index['folio_group'] = list();
    for doc_id in range(0,file_count):
      filename = self.pathManager_obj[self.dsetname].displayPath(doc_id);
      self.filename_to_docid_map[filename] = doc_id;
      istc_id, mei_id, folio = self.extract_filename_parts(filename);
      dataset_index['filename'].append(filename);
      dataset_index['istc_id'].append(istc_id);
      dataset_index['mei_id'].append(mei_id);
      dataset_index['folio'].append(folio);
      dataset_index['folio_group'].append(folio[0]);

    if file_attributes_fn != None:
      csv_metadata = pd.read_csv(file_attributes_fn, encoding='utf-8');
      if file_attributes_filename_colname != 'filename':
        csv_metadata.rename( columns={file_attributes_filename_colname: 'filename'}, inplace=True );
      csv_metadata.drop([col for col in csv_metadata.columns if "Unnamed" in col], axis=1, inplace=True) # remove unnamed columns
      dataset_index_df = pd.DataFrame(dataset_index);

      self.file_attributes_index = pd.merge(dataset_index_df, csv_metadata, on='filename')
      self.attributes_available = True;
    else:
      self.file_attributes_index = pd.DataFrame(dataset_index);

    print 'Finished loading attributes for %d files' % (self.file_attributes_index.shape[0])

  def filename_to_docid(self, filename_pattern):
    match = self.file_attributes_index[ self.file_attributes_index['filename'].str.contains(filename_pattern) ]
    return match.iloc[:]['doc_id']

  def extract_filename_parts(self, filename):
    # filename can be in two formats
    # ia00152000_00202205_i2v.jpg, ia00149000_00200293 a2r.jpg
    #filename = row['filename'];
    istc_id = '';
    mei_id = '';
    folio = '';
    dot_index = filename.rfind('.');
    if filename.count('_') == 2:
      # format 1 : ia00152000_00202205_i2v.jpg
        s1 = filename.find('_', 0);
        s2 = filename.find('_', s1 + 1);
        istc_id = filename[ 0 : s1 ]; 
        mei_id  = filename[ (s1+1) : s2 ];
        folio   = filename[ (s2+1) : dot_index ];
    else:
        if filename.count('_') == 1 and filename.count(' ') == 1:
          # format 2 : ia00149000_00200293 a2r.jpg
          s1 = filename.find('_', 0);
          s2 = filename.find(' ', s1 + 1);
          istc_id = filename[ 0 : s1 ]; 
          mei_id  = filename[ (s1+1) : s2 ];
          folio   = filename[ (s2+1) : dot_index ];
        else:
          # for all other formats, discard folio
          s1 = filename.find('_', 0);
          istc_id = filename[ 0 : s1 ]; 
          mei_id  = filename[ (s1+1) : (s1+8) ];
          folio   = ''

    if folio == '':
      folio = '_UNKNOWN_';
    return istc_id, mei_id, folio;

  def get_file_metadata(self, docID=None, filename=None):
    if docID == None and filename == None:
      print('Error: get_file_metadata() must be provided with either docID or filename')
      return;

    if filename == None:
      filename = self.pathManager_obj[self.dsetname].displayPath(docID)

    istc_id, mei_id, folio = self.extract_filename_parts(filename);
    istc_metadata = self.istc_db[ self.istc_db['id'] == istc_id ]
    return istc_metadata;

  def get_istc_metadata(self, istc_id):
    istc_metadata = self.istc_db[ self.istc_db['id'] == istc_id ]
    return istc_metadata;

  def get_istc_metadata_html_table(self, filename):
      istc_metadata = self.get_file_metadata(filename=filename);
      html = 'ISTC metadata for ' + filename + ' not found!';
      if istc_metadata.shape[0] == 1:
          html = '<table class="metadata_table"><tbody>'
          for key in istc_metadata:
              value = istc_metadata.iloc[0][key]
              if key == 'id':
                  istc_url = '<a target="_blank" href="http://data.cerl.org/istc/%s">%s</a>' % (value, value);
                  html += '<tr><td>%s</td><td>%s</td></tr>' % (key, istc_url);
                  continue;
              if value != '':
                  html += '<tr><td>%s</td><td>%s</td></tr>' % (key, value);
              else:
                  html += '<tr><td>%s</td><td></td></tr>' % (key);
          html += '</tbody></table>';
      return html;

  def get_region_attribute_html(self, region, rank, doc_id, region_spec):
    # @todo: transform the region spec to scaled image (i.e. preprocess_log.csv)
    if region_spec == '':
      region_img = '';
      usr_msg = 'Not showing non-rectangular region';
    else:
      #region_img = '<div class="img_panel"><img src="getImage?docID=%d&height=200&%s&crop"></div>' % (doc_id, region_spec);
      region_img = '';
      usr_msg = '';

    region_metadata_html = '''
<div class="search_result_i pagerow">
  <div class="header">
    <span>Region: %d</span>
    <span style="color: red">%s</span>
  </div>
  %s
  <div class="istc_metadata"><strong>Region Metadata</strong>
    <input type="checkbox" class="show_more_state" id="region_%d_metadata" />
    <table class="metadata_table show_more_wrap"><tbody>
''' % (rank+1, usr_msg, region_img , rank);

    region_json = json.loads(region);
    metadata_index = 0;
    for key in region_json :
      value = region_json[key];
      if value != '':
        if metadata_index < 4:
          region_metadata_html += '<tr><td>%s</td><td>%s</td></tr>' % (key, value);
        else:
          region_metadata_html += '<tr class="show_more_target"><td>%s</td><td>%s</td></tr>' % (key, value);
      else:
          region_metadata_html += "<tr><td>%s</td><td></td></tr>" % (key);

      metadata_index = metadata_index + 1;

#http://localhost:9981/15cILLUSTRATION/getImage?docID=237&height=200&xl=267.00&xu=843.00&yl=450.00&yu=1023.00&H=1.030070,0.000000,-36.005528,0.022533,0.970808,-88.125657,0.000000,0.000000,1.000000&crop

    # add the corresponding region (crop of image) for visualization

    # end of for metadata_i
    region_metadata_html += '</tbody></table><label for="region_%d_metadata" class="show_more_trigger"></label>' % (rank);
    # @todo: show scaled region
    #region_metadata_html += '<div><img src="getImage?docID=%d&%s&crop"></div>' % (doc_id, region_spec);
    region_metadata_html += '</div></div>';

    return region_metadata_html;

  def get_filename_info_html(self, doc_id):
    doc_id = int(doc_id);
    filename = self.pathManager_obj[self.dsetname].displayPath(doc_id)
    istc_metadata = self.get_istc_metadata_html_table(filename);

    all_region_metadata = '';
    regions = self.region_attributes[ self.region_attributes['filename'] == filename ];
    for index, row in regions.iterrows():
      filename = row['filename'];
      shape_attributes_str = row['region_shape_attributes'];
      sa = json.loads(shape_attributes_str);
      if sa['name'] == 'rect':
        rx = int(sa['x']);
        ry = int(sa['y']);
        rw = int(sa['width']);
        rh = int(sa['height']);
        #
        # @todo: transform the region spec to scaled image (i.e. preprocess_log.csv)
        #
        region_spec = 'xl=%s&xu=%s&yl=%s&yu=%s&H=1,0,0,0,1,0,0,0,1' % (rx, rx+rw, ry, ry+rh);
      else:
        region_spec = '';

      all_region_metadata += self.get_region_attribute_html( row['region_attributes'], row['region_id'], doc_id, region_spec );

    return '''
<div class="search_result_i pagerow">
<div class="header">
  <span>Filename: %s</span>
  <span></span>
</div>

<div class="img_panel">
  <a href="search?docID=%d"><img src="getImage?docID=%d&height=500"></a>
</div>
<div class="istc_metadata"><strong>ISTC Metadata</strong>%s</div>

<div class="search_result_tools">
<ul class="hlist">
  <li><a href="search?docID=%d">Search using this image</a></li>
</ul>
</div>
</div>
%s''' % (filename, doc_id, doc_id, istc_metadata, doc_id, all_region_metadata);

  @cherrypy.expose
  def index(self, docID= None, filename=None):
    doc_id_list = pd.Series( data=[], dtype=int );
    body = '''
<div class="istc_search_panel pagerow">
  <form method="GET" action="./file_attributes" id="filename_search">
    <input type="text" id="filename_search_keyword" name="filename" placeholder="Image filename (e.g. i3v)" size="25">
    <button type="submit" value="Search">Search</button>
  </form>
</div>''';

    if docID is not None:
      body += self.get_filename_info_html( docID );
    else:
      if filename is not None:
        doc_id_list = self.filename_to_docid(filename);
        if doc_id_list.size == 1:
          body += self.get_filename_info_html( doc_id_list.iloc[0] );
        else:
          # show a list of matching filename
          filename_list_html = '<p>No match found for keyword "%s"</p>' % (filename);

          if doc_id_list.size != 0:
            filename_list_html = '<div class="header"><span>Showing %d matches</span><span></span></div><ul>' % (doc_id_list.size);
            for doc_id in doc_id_list:
              match = self.file_attributes_index[ self.file_attributes_index['doc_id'] == doc_id ]
              #filename_list_html += '<li><a title="Search using this image" href="../search?docID=%d">%.5d</a> : <a title="View image attributes" href="file_attributes?docID=%d">%s</a></li>' % (doc_id, doc_id, doc_id, match.iloc[0]['filename']);
              filename_list_html += '<li><a title="View image attributes" href="file_attributes?docID=%d">%s</a></li>' % (doc_id, match.iloc[0]['filename']);
            filename_list_html += '</ul>';
          body += '<div class="search_result_i pagerow">%s</div>' % (filename_list_html);


    return self.pT.get( title='Filename search', body=body, headExtra='' );
