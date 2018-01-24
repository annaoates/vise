/*
 * Javascript code to drive the user interface of VGG Image Search Engine (VISE)
 *
 * Author: Abhishek Dutta < adutta@robots.ox.ac.uk>
 * 24 Jan. 2018
 *
 */

function toggle_hidden_search_result() {
  var sr = document.getElementsByClassName('search_result_i');
  var n = sr.length;
  var seen_first_hidden_block = false;
  for( var i=0; i < n; i++ ) {
    if( sr[i].classList.contains('hidden_by_default') ) {
      sr[i].classList.toggle('display-none');

      if( !seen_first_hidden_block ) {
        seen_first_hidden_block = true;
        sr[i].scrollIntoView();
      }
    }
  }
}
