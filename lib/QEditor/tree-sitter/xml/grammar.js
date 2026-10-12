/**
 * @file XML grammar for tree-sitter.
 *
 * Covers what the editor needs to highlight well-formed XML documents:
 * elements, attributes, text, entity references, comments, CDATA sections,
 * processing instructions, the XML declaration and DOCTYPE declarations.
 * The internal DTD subset of a DOCTYPE is treated as opaque text.
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: 'xml',

  extras: $ => [/\s+/],

  rules: {
    document: $ => repeat($._node),

    _node: $ => choice(
      $.xml_declaration,
      $.processing_instruction,
      $.doctype,
      $.comment,
      $.element,
      $.text,
      $.entity_reference,
      $.cdata_section,
    ),

    // The trailing whitespace is part of the token so that "<?xml" is never
    // mistaken for the start of a processing instruction named "xml-...".
    xml_declaration: $ => seq(
      alias(token(prec(2, /<\?xml[ \t\r\n]/)), $.declaration_start),
      repeat($.attribute),
      '?>',
    ),

    processing_instruction: $ => seq(
      '<?',
      $.name,
      optional($.processing_instruction_value),
      '?>',
    ),

    processing_instruction_value: _ => token(/([^?]|\?[^>])+/),

    doctype: _ => token(seq('<!DOCTYPE', /[^>]*/, '>')),

    comment: _ => token(seq('<!--', /([^-]|-[^-])*/, '-->')),

    cdata_section: _ => token(seq(
      '<![CDATA[',
      /([^\]]|\][^\]]|\]\][^>])*/,
      ']]>',
    )),

    element: $ => choice(
      seq($.start_tag, repeat($._content), $.end_tag),
      $.empty_element,
    ),

    start_tag: $ => seq('<', $.name, repeat($.attribute), '>'),

    empty_element: $ => seq('<', $.name, repeat($.attribute), '/>'),

    end_tag: $ => seq('</', $.name, '>'),

    _content: $ => choice(
      $.element,
      $.text,
      $.entity_reference,
      $.comment,
      $.cdata_section,
      $.processing_instruction,
    ),

    // Element and attribute names share one token, the parser tells them
    // apart by position.
    name: _ => /[A-Za-z_:][A-Za-z0-9_.:-]*/,

    attribute: $ => seq(
      field('name', $.name),
      '=',
      field('value', $.attribute_value),
    ),

    attribute_value: _ => choice(
      token(seq('"', /[^"]*/, '"')),
      token(seq("'", /[^']*/, "'")),
    ),

    entity_reference: _ => token(
      /&(#[0-9]+|#x[0-9A-Fa-f]+|[A-Za-z_][A-Za-z0-9_.-]*);/,
    ),

    // Starts with a non-space character; leading whitespace is an extra.
    text: _ => token(/[^<&\s][^<&]*/),
  },
});
