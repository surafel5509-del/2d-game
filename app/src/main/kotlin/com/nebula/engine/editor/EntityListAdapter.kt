package com.nebula.engine.editor

import android.graphics.Typeface
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.nebula.engine.R

data class EntityItem(val id: Int, val name: String, val tag: String)

/**
 * Hierarchy list adapter. Selection state is driven by [selectedId];
 * taps are routed back through [onSelect].
 */
class EntityListAdapter(
    private val onSelect: (EntityItem) -> Unit
) : RecyclerView.Adapter<EntityListAdapter.ViewHolder>() {

    private val items = mutableListOf<EntityItem>()
    var selectedId: Int = 0
        set(value) {
            val old = field
            field = value
            items.forEachIndexed { index, item ->
                if (item.id == old || item.id == value) notifyItemChanged(index)
            }
        }

    fun submit(newItems: List<EntityItem>) {
        items.clear()
        items.addAll(newItems)
        notifyDataSetChanged()
    }

    override fun getItemCount(): Int = items.size

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.item_entity, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.bind(items[position], items[position].id == selectedId)
    }

    inner class ViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val root: View = itemView.findViewById(R.id.itemRoot)
        private val dot: View = itemView.findViewById(R.id.viewDot)
        private val label: TextView = itemView.findViewById(R.id.tvEntityName)

        fun bind(item: EntityItem, selected: Boolean) {
            itemView.isSelected = selected
            root.isSelected = selected
            label.text = if (item.tag.isNotEmpty()) "${item.name}  ·  ${item.tag}" else item.name
            label.setTypeface(label.typeface, if (selected) Typeface.BOLD else Typeface.NORMAL)
            dot.visibility = if (selected) View.VISIBLE else View.INVISIBLE
            itemView.setOnClickListener { onSelect(item) }
        }
    }
}
